/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2012 Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>

#include <poll.h>

#include <freetds/tds.h>
#include <freetds/bytes.h>
#include <freetds/iconv.h>
#include <freetds/replacements.h>
#include <freetds/tls.h>

static TDSRET tds_update_recv_wnd(TDSSOCKET *tds, TDS_UINT new_recv_wnd);
static int tds_packet_write(TDSCONNECTION *conn);

/* get packet from the cache */
static TDSPACKET *
tds_get_packet(TDSCONNECTION *conn, unsigned len)
{
	TDSPACKET *packet, *to_free = NULL;

	tds_mutex_lock(&conn->list_mtx);
	while ((packet = conn->packet_cache) != NULL) {
		--conn->num_cached_packets;
		conn->packet_cache = packet->next;

		/* return it */
		if (packet->capacity >= len) {
			packet->next = NULL;
			tds_packet_zero_data_start(packet);
			packet->data_len = 0;
			packet->sid = 0;
			break;
		}

		/* discard packet if too small */
		packet->next = to_free;
		to_free = packet;
	}
	tds_mutex_unlock(&conn->list_mtx);

	if (to_free)
		tds_free_packets(to_free);

	if (!packet)
		packet = tds_alloc_packet(NULL, len);

	return packet;
}

/* append packets in cached list. must have the lock! */
static void
tds_packet_cache_add(TDSCONNECTION *conn, TDSPACKET *packet)
{
	TDSPACKET *last;
	unsigned count = 1;

	assert(conn && packet);
	tds_mutex_check_owned(&conn->list_mtx);

	if (conn->num_cached_packets >= 8) {
		tds_free_packets(packet);
		return;
	}

	for (last = packet; last->next; last = last->next)
		++count;

	last->next = conn->packet_cache;
	conn->packet_cache = packet;
	conn->num_cached_packets += count;
}

static void
tds_append_packet(TDSPACKET **p_packet, TDSPACKET *packet)
{
	while (*p_packet)
		p_packet = &((*p_packet)->next);
	*p_packet = packet;
}

/**
 * Read in one 'packet' from the server.  This is a wrapped outer packet of
 * the protocol (they bundle result packets into chunks and wrap them at
 * what appears to be 512 bytes regardless of how that breaks internal packet
 * up.   (tetherow\@nol.org)
 * @return bytes read or -1 on failure
 */
int
tds_read_packet(TDSSOCKET * tds)
{
#if ENABLE_ODBC_MARS
#else /* !ENABLE_ODBC_MARS */
	unsigned char *pkt = tds->in_buf, *p, *end;

	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_NETWORK, "Read attempt when state is TDS_DEAD");
		return -1;
	}

	tds->in_len = 0;
	tds->in_pos = 0;
	for (p = pkt, end = p+8; p < end;) {
		int len = tds_connection_read(tds, p, end - p);
		if (len <= 0) {
			tds_close_socket(tds);
			return -1;
		}

		p += len;
		if (p - pkt >= 4) {
			unsigned pktlen = TDS_GET_A2BE(pkt+2);
			/* packet must at least contains header */
			if (TDS_UNLIKELY(pktlen < 8)) {
				tds_close_socket(tds);
				return -1;
			}
			if (TDS_UNLIKELY(pktlen > tds->recv_packet->capacity)) {
				TDSPACKET *packet = tds_realloc_packet(tds->recv_packet, pktlen);
				if (TDS_UNLIKELY(!packet)) {
					tds_close_socket(tds);
					return -1;
				}
				tds->recv_packet = packet;
				pkt = packet->buf;
				p = pkt + (p-tds->in_buf);
				tds->in_buf = pkt;
			}
			end = pkt + pktlen;
		}
	}

	/* set the received packet type flag */
	tds->in_flag = pkt[0];

	/* Set the length and pos (not sure what pos is used for now */
	tds->in_len = p - pkt;
	tds->in_pos = 8;
	tdsdump_dump_buf(TDS_DBG_NETWORK, "Received packet", tds->in_buf, tds->in_len);

	return tds->in_len;
#endif /* !ENABLE_ODBC_MARS */
}

#if ENABLE_ODBC_MARS
static TDSRET
tds_update_recv_wnd(TDSSOCKET *tds, TDS_UINT new_recv_wnd)
{
	TDS72_SMP_HEADER *mars;
	TDSPACKET *packet;

	if (!tds->conn->mars)
		return TDS_SUCCESS;

	packet = tds_get_packet(tds->conn, sizeof(*mars));
	if (!packet)
		return TDS_FAIL;	/* TODO check result */

	packet->data_len = sizeof(*mars);
	packet->sid = tds->sid;

	mars = (TDS72_SMP_HEADER *) packet->buf;
	mars->signature = TDS72_SMP;
	mars->type = TDS_SMP_ACK;
	TDS_PUT_A2LE(&mars->sid, tds->sid);
	mars->size = TDS_HOST4LE(16);
	TDS_PUT_A4LE(&mars->seq, tds->send_seq);
	tds->recv_wnd = new_recv_wnd;
	TDS_PUT_A4LE(&mars->wnd, tds->recv_wnd);

	tds_mutex_lock(&tds->conn->list_mtx);
	tds_append_packet(&tds->conn->send_packets, packet);
	tds_mutex_unlock(&tds->conn->list_mtx);

	return TDS_SUCCESS;
}

static TDSRET
tds_append_fin_syn(TDSSOCKET *tds, uint8_t type)
{
	TDS72_SMP_HEADER mars;
	TDSPACKET *packet;

	if (!tds->conn->mars)
		return TDS_SUCCESS;

	mars.signature = TDS72_SMP;
	mars.type = type;
	TDS_PUT_A2LE(&mars.sid, tds->sid);
	mars.size = TDS_HOST4LE(16);
	TDS_PUT_A4LE(&mars.seq, tds->send_seq);
	tds->recv_wnd = tds->recv_seq + 4;
	TDS_PUT_A4LE(&mars.wnd, tds->recv_wnd);

	/* do not use tds_get_packet as it require no lock ! */
	packet = tds_alloc_packet(&mars, sizeof(mars));
	if (!packet)
		return TDS_FAIL;	/* TODO check result */
	packet->sid = tds->sid;

	/* we already hold lock so do not lock */
	tds_append_packet(&tds->conn->send_packets, packet);

	if (type == TDS_SMP_FIN) {
		/* now is no more an active session */
		tds->conn->sessions[tds->sid] = BUSY_SOCKET;
		tds_set_state(tds, TDS_DEAD);
	}

	return TDS_SUCCESS;
}

/**
 * Append a SMP FIN packet.
 * tds->conn->list_mtx must be locked.
 */
TDSRET
tds_append_fin(TDSSOCKET *tds)
{
	return tds_append_fin_syn(tds, TDS_SMP_FIN);
}

/**
 * Append a SMP SYN packet.
 * tds->conn->list_mtx must be unlocked.
 */
TDSRET
tds_append_syn(TDSSOCKET *tds)
{
	TDSRET ret;
	tds_mutex_lock(&tds->conn->list_mtx);
	ret = tds_append_fin_syn(tds, TDS_SMP_SYN);
	tds_mutex_unlock(&tds->conn->list_mtx);
	return ret;
}
#endif /* ENABLE_ODBC_MARS */


TDSRET
tds_write_packet(TDSSOCKET * tds, unsigned char final)
{
	TDSRET res;
	unsigned int left = 0;
	TDSPACKET *pkt = tds->send_packet, *pkt_next = NULL;

#if !ENABLE_ODBC_MARS
	if (tds->frozen)
#endif
	{
		pkt->next = pkt_next = tds_get_packet(tds->conn, pkt->capacity);
		if (!pkt_next)
			return TDS_FAIL;

#if ENABLE_ODBC_MARS
		if (tds->conn->mars)
			pkt_next->data_start = sizeof(TDS72_SMP_HEADER);
#endif
	}

	if (tds->out_pos > tds->out_buf_max) {
		left = tds->out_pos - tds->out_buf_max;
		if (pkt_next)
			memcpy(pkt_next->buf + tds_packet_get_data_start(pkt_next) + 8, tds->out_buf + tds->out_buf_max, left);
		tds->out_pos = tds->out_buf_max;
	}

	/* we must assure server can accept our packet looking at
	 * send_wnd and waiting for proper send_wnd if send_seq > send_wnd
	 */
	tds->out_buf[0] = tds->out_flag;
	tds->out_buf[1] = final;
	TDS_PUT_A2BE(tds->out_buf+2, tds->out_pos);
	TDS_PUT_A2BE(tds->out_buf+4, tds->conn->client_spid);
	TDS_PUT_A2(tds->out_buf+6, 0);
	if (IS_TDS7_PLUS(tds->conn) && !tds->login)
		tds->out_buf[6] = 0x01;

	if (tds->frozen) {
		pkt->data_len = tds->out_pos;
		tds_set_current_send_packet(tds, pkt_next);
		tds->out_pos = left + 8;
		return TDS_SUCCESS;
	}

	tdsdump_dump_buf(TDS_DBG_NETWORK, "Sending packet", tds->out_buf, tds->out_pos);

	/* GW added in check for write() returning <0 and SIGPIPE checking */
	res = tds_connection_write(tds, tds->out_buf, tds->out_pos, final) <= 0 ?
		TDS_FAIL : TDS_SUCCESS;

	memcpy(tds->out_buf + 8, tds->out_buf + tds->out_buf_max, left);

	tds->out_pos = left + 8;

	if (TDS_UNLIKELY(tds->conn->encrypt_single_packet)) {
		tds->conn->encrypt_single_packet = 0;
		tds_ssl_deinit(tds->conn);
	}

	return res;
}

#if !ENABLE_ODBC_MARS
int
tds_put_cancel(TDSSOCKET * tds)
{
	unsigned char out_buf[8];
	int sent;

	out_buf[0] = TDS_CANCEL;	/* out_flag */
	out_buf[1] = 1;	/* final */
	out_buf[2] = 0;
	out_buf[3] = 8;
	TDS_PUT_A4(out_buf+4, 0);
	if (IS_TDS7_PLUS(tds->conn) && !tds->login)
		out_buf[6] = 0x01;

	tdsdump_dump_buf(TDS_DBG_NETWORK, "Sending packet", out_buf, 8);

	sent = tds_connection_write(tds, out_buf, 8, 1);

	if (sent > 0)
		tds->in_cancel = 2;

	/* GW added in check for write() returning <0 and SIGPIPE checking */
	return sent <= 0 ? TDS_FAIL : TDS_SUCCESS;
}
#endif /* !ENABLE_ODBC_MARS */


#if ENABLE_ODBC_MARS
static int
tds_packet_write(TDSCONNECTION *conn)
{
	int sent;
	int final;
	TDSPACKET *packet = conn->send_packets;

	assert(packet);

	if (conn->send_pos == 0)
		tdsdump_dump_buf(TDS_DBG_NETWORK, "Sending packet", packet->buf, packet->data_start + packet->data_len);

	/* take into account other session packets */
	if (packet->next != NULL)
		final = 0;
	/* take into account other packets for this session */
	else if (packet->buf[0] != TDS72_SMP)
		final = packet->buf[1] & 1;
	else
		final = 1;

	sent = tds_connection_write(conn->in_net_tds, packet->buf + conn->send_pos,
				    packet->data_start + packet->data_len - conn->send_pos, final);

	if (TDS_UNLIKELY(sent < 0)) {
		/* TODO tdserror called ?? */
		tds_connection_close(conn);
		return -1;
	}

	/* update sent data */
	conn->send_pos += sent;
	/* remove packet if sent all data */
	if (conn->send_pos >= packet->data_start + packet->data_len) {
		uint16_t sid = packet->sid;
		TDSSOCKET *tds;
		tds_mutex_lock(&conn->list_mtx);
		tds = conn->sessions[sid];
		if (TDSSOCKET_VALID(tds) && tds->sending_packet == packet)
			tds->sending_packet = NULL;
		conn->send_packets = packet->next;
		packet->next = NULL;
		tds_packet_cache_add(conn, packet);
		tds_mutex_unlock(&conn->list_mtx);
		conn->send_pos = 0;
		return sid;
	}

	return -1;
}
#endif /* ENABLE_ODBC_MARS */

/**
 * Stop writing to server and cache every packet not sending them to server.
 * This is used to write data without worrying to compute length before.
 * If size_len is provided the number of bytes written between ::tds_freeze and
 * ::tds_freeze_close will be written as a number of size size_len.
 * This call should be followed by a ::tds_freeze_close, ::tds_freeze_close_len or
 * a ::tds_freeze_abort. Failing to match ::tds_freeze with a close would possibly
 * result in a disconnection from the server.
 *
 * @param[out]  freeze    structure to initialize
 * @param       size_len  length of the size to automatically write on close (0, 1, 2, or 4)
 */
void
tds_freeze(TDSSOCKET *tds, TDSFREEZE *freeze, unsigned size_len)
{
  if (tds->out_pos > tds->out_buf_max)
		tds_write_packet(tds, 0x0);

	if (!tds->frozen)
		tds->frozen_packets = tds->send_packet;

	++tds->frozen;
	freeze->tds = tds;
	freeze->pkt = tds->send_packet;
	freeze->pkt_pos = tds->out_pos;
	freeze->size_len = size_len;
	if (size_len)
		tds_put_n(tds, NULL, size_len);
}

/**
 * Compute how many bytes has been written from freeze
 *
 * @return bytes written since ::tds_freeze call
 */
size_t
tds_freeze_written(TDSFREEZE *freeze)
{
	TDSSOCKET *tds = freeze->tds;
	TDSPACKET *pkt = freeze->pkt;
	size_t size;

	/* last packet needs special handling */
	size = tds->out_pos;

	/* packets before last */
	for (; pkt->next != NULL; pkt = pkt->next)
		size += pkt->data_len - 8;

	return size - freeze->pkt_pos;
}

/**
 * Discard all data written after the freeze
 *
 * After this call freeze should not be used.
 *
 * @param[in]  freeze  structure to work on
 */
TDSRET
tds_freeze_abort(TDSFREEZE *freeze)
{
	TDSSOCKET *tds = freeze->tds;
	TDSPACKET *pkt = freeze->pkt;

	if (pkt->next) {
		tds_mutex_lock(&tds->conn->list_mtx);
		tds_packet_cache_add(tds->conn, pkt->next);
		tds_mutex_unlock(&tds->conn->list_mtx);
		pkt->next = NULL;

		tds_set_current_send_packet(tds, pkt);
	}
	tds->out_pos = freeze->pkt_pos;
	pkt->data_len = 8;

	--tds->frozen;
	if (!tds->frozen)
		tds->frozen_packets = NULL;
	freeze->tds = NULL;
	return TDS_SUCCESS;
}

/**
 * Stop keeping data for this specific freeze.
 *
 * If size_len was used for ::tds_freeze this function write the written bytes
 * at position when ::tds_freeze was called.
 * After this call freeze should not be used.
 *
 * @param[in]  freeze  structure to work on
 */
TDSRET
tds_freeze_close(TDSFREEZE *freeze)
{
	return tds_freeze_close_len(freeze, freeze->size_len ? tds_freeze_written(freeze) - freeze->size_len : 0);
}

static void
tds_freeze_update_size(const TDSFREEZE *freeze, int32_t size)
{
	TDSPACKET *pkt;
	unsigned pos = freeze->pkt_pos;
	unsigned size_len = freeze->size_len;

	pkt = freeze->pkt;
	do {
		if (pos >= pkt->data_len && pkt->next) {
			pkt = pkt->next;
			pos = 8;
		}
		pkt->buf[tds_packet_get_data_start(pkt) + pos] = size & 0xffu;
		size >>= 8;
		pos++;
	} while (--size_len);
}

/**
 * Stop keeping data for this specific freeze.
 *
 * Similar to ::tds_freeze_close but specify the size to be written instead
 * of letting ::tds_freeze_close compute it.
 * After this call freeze should not be used.
 *
 * @param[in]  freeze  structure to work on
 * @param[in]  size    size to write
 */
TDSRET
tds_freeze_close_len(TDSFREEZE *freeze, int32_t size)
{
	TDSSOCKET *tds = freeze->tds;
	TDSPACKET *pkt;
  TDSPACKET *last_pkt_sent = NULL;

	if (freeze->size_len)
		tds_freeze_update_size(freeze, size);

	/* if not last freeze we need just to update size */
	freeze->tds = NULL;
	if (--tds->frozen != 0)
		return TDS_SUCCESS;

	tds->frozen_packets = NULL;
	pkt = freeze->pkt;
	while (pkt->next) {
		TDSPACKET *next = pkt->next;
		TDSRET rc;
#if ENABLE_ODBC_MARS
#else
		rc = tds_connection_write(tds, pkt->buf, pkt->data_len, 0) <= 0 ?
			TDS_FAIL : TDS_SUCCESS;
		last_pkt_sent = pkt;
#endif
		if (TDS_UNLIKELY(TDS_FAILED(rc))) {
			while (next->next) {
				pkt = next;
				next = pkt->next;
			}

			pkt->next = NULL;
			tds_mutex_lock(&tds->conn->list_mtx);
			tds_packet_cache_add(tds->conn, freeze->pkt);
			tds_mutex_unlock(&tds->conn->list_mtx);
			return rc;
		}
		pkt = next;
	}

  if (last_pkt_sent) {
		last_pkt_sent->next = NULL;
		tds_mutex_lock(&tds->conn->list_mtx);
		tds_packet_cache_add(tds->conn, freeze->pkt);
		tds_mutex_unlock(&tds->conn->list_mtx);
	}

	/* keep final packet so we can continue to add data */
	return TDS_SUCCESS;
}

/** @} */
