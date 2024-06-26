/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2005-2015  Frediano Ziglio
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


#include <stdio.h>
#include <string.h>

#include <freetds/tds.h>
#include <freetds/convert.h>
#include <freetds/bytes.h>

/**
 * tds_numeric_bytes_per_prec is indexed by precision and will
 * tell us the number of bytes required to store the specified
 * precision (with the sign).
 * Support precision up to 77 digits
 */
const int tds_numeric_bytes_per_prec[] = {
	/*
	 * precision can't be 0 but using a value > 0 assure no
	 * core if for some bug it's 0...
	 */
	1, 
	2,  2,  3,  3,  4,  4,  4,  5,  5,
	6,  6,  6,  7,  7,  8,  8,  9,  9,  9,
	10, 10, 11, 11, 11, 12, 12, 13, 13, 14,
	14, 14, 15, 15, 16, 16, 16, 17, 17, 18,
	18, 19, 19, 19, 20, 20, 21, 21, 21, 22,
	22, 23, 23, 24, 24, 24, 25, 25, 26, 26,
	26, 27, 27, 28, 28, 28, 29, 29, 30, 30,
	31, 31, 31, 32, 32, 33, 33, 33
};

/*
 * money is a special case of numeric really...that why its here
 */
char *
tds_money_to_string(const TDS_MONEY * money, char *s, bool use_2_digits)
{
	TDS_INT8 mymoney;
	TDS_UINT8 n;
	char *p;

	/* sometimes money it's only 4-byte aligned so always compute 64-bit */
	mymoney = (((TDS_INT8) money->tdsoldmoney.mnyhigh) << 32) | money->tdsoldmoney.mnylow;

	p = s;
	if (mymoney < 0) {
		*p++ = '-';
		/* we use unsigned cause this cause arithmetic problem for -2^63*/
		n = -mymoney;
	} else {
		n = mymoney;
	}
	/* if machine is 64 bit you do not need to split n */
	if (use_2_digits) {
		n = (n+ 50) / 100;
		sprintf(p, "%" PRIu64 ".%02u", n / 100u, (unsigned) (n % 100u));
	} else {
		sprintf(p, "%" PRIu64 ".%04u", n / 10000u, (unsigned) (n % 10000u));
	}
	return s;
}

/**
 * @return <0 if error
 */
TDS_INT
tds_numeric_to_string(const TDS_NUMERIC * numeric, char *s)
{
	const unsigned char *number;

	unsigned int packet[sizeof(numeric->array) / 2];
	unsigned int *pnum, *packet_start;
	unsigned int *const packet_end = packet + TDS_VECTOR_SIZE(packet);

	unsigned int packet10k[(MAXPRECISION + 3) / 4];
	unsigned int *p;

	int num_bytes;
	unsigned int remainder, n, i, m;

	/* a bit of debug */
#if ENABLE_EXTRA_CHECKS
	memset(packet, 0x55, sizeof(packet));
	memset(packet10k, 0x55, sizeof(packet10k));
#endif

	if (numeric->precision < 1 || numeric->precision > MAXPRECISION || numeric->scale > numeric->precision)
		return TDS_CONVERT_FAIL;

	/* set sign */
	if (numeric->array[0] == 1)
		*s++ = '-';

	/* put number in a 16bit array */
	number = numeric->array;
	num_bytes = tds_numeric_bytes_per_prec[numeric->precision];

	n = num_bytes - 1;
	pnum = packet_end;
	for (; n > 1; n -= 2)
		*--pnum = TDS_GET_UA2BE(&number[n - 1]);
	if (n == 1)
		*--pnum = number[n];
	while (!*pnum) {
		++pnum;
		if (pnum == packet_end) {
			*s++ = '0';
			if (numeric->scale) {
				*s++ = '.';
				i = numeric->scale;
				do {
					*s++ = '0';
				} while (--i);
			}
			*s = 0;
			return 1;
		}
	}
	packet_start = pnum;

	/* transform 2^16 base number in 10^4 base number */
	for (p = packet10k + TDS_VECTOR_SIZE(packet10k); packet_start != packet_end;) {
		pnum = packet_start;
		n = *pnum;
		remainder = n % 10000u;
		if (!(*pnum++ = (n / 10000u)))
			packet_start = pnum;
		for (; pnum != packet_end; ++pnum) {
			n = remainder * (256u * 256u) + *pnum;
			remainder = n % 10000u;
			*pnum = n / 10000u;
		}
		*--p = remainder;
	}

	/* transform to 10 base number and output */
	i = 4 * (unsigned int)((packet10k + TDS_VECTOR_SIZE(packet10k)) - p);	/* current digit */
	/* skip leading zeroes */
	n = 1000;
	remainder = *p;
	while (remainder < n)
		n /= 10, --i;
	if (i <= numeric->scale) {
		*s++ = '0';
		*s++ = '.';
		m = i;
		while (m < numeric->scale)
			*s++ = '0', ++m;
	}
	for (;;) {
		*s++ = (remainder / n) + '0';
		--i;
		remainder %= n;
		n /= 10;
		if (!n) {
			n = 1000;
			if (++p == packet10k + TDS_VECTOR_SIZE(packet10k))
				break;
			remainder = *p;
		}
		if (i == numeric->scale)
			*s++ = '.';
	}
	*s = 0;

	return 1;
}

#define TDS_WORD  uint32_t
#define TDS_DWORD uint64_t
#define TDS_WORD_DDIGIT 9

// Determine if this can be eliminated
#define LIMIT_INDEXES_ADJUST 4

static const signed char limit_indexes[79]= {
  0,	/*  0 */
  -3,	/*  1 */
  -6,	/*  2 */
  -9,	/*  3 */
  -12,	/*  4 */
  -15,	/*  5 */
  -18,	/*  6 */
  -21,	/*  7 */
  -24,	/*  8 */
  -27,	/*  9 */
  -30,	/* 10 */
  -32,	/* 11 */
  -34,	/* 12 */
  -36,	/* 13 */
  -38,	/* 14 */
  -40,	/* 15 */
  -42,	/* 16 */
  -44,	/* 17 */
  -46,	/* 18 */
  -48,	/* 19 */
  -50,	/* 20 */
  -51,	/* 21 */
  -52,	/* 22 */
  -53,	/* 23 */
  -54,	/* 24 */
  -55,	/* 25 */
  -56,	/* 26 */
  -57,	/* 27 */
  -58,	/* 28 */
  -59,	/* 29 */
  -59,	/* 30 */
  -59,	/* 31 */
  -59,	/* 32 */
  -60,	/* 33 */
  -61,	/* 34 */
  -62,	/* 35 */
  -63,	/* 36 */
  -64,	/* 37 */
  -65,	/* 38 */
  -66,	/* 39 */
  -66,	/* 40 */
  -66,	/* 41 */
  -66,	/* 42 */
  -66,	/* 43 */
  -66,	/* 44 */
  -66,	/* 45 */
  -66,	/* 46 */
  -66,	/* 47 */
  -66,	/* 48 */
  -66,	/* 49 */
  -65,	/* 50 */
  -64,	/* 51 */
  -63,	/* 52 */
  -62,	/* 53 */
  -61,	/* 54 */
  -60,	/* 55 */
  -59,	/* 56 */
  -58,	/* 57 */
  -57,	/* 58 */
  -55,	/* 59 */
  -53,	/* 60 */
  -51,	/* 61 */
  -49,	/* 62 */
  -47,	/* 63 */
  -45,	/* 64 */
  -44,	/* 65 */
  -43,	/* 66 */
  -42,	/* 67 */
  -41,	/* 68 */
  -39,	/* 69 */
  -37,	/* 70 */
  -35,	/* 71 */
  -33,	/* 72 */
  -31,	/* 73 */
  -29,	/* 74 */
  -27,	/* 75 */
  -25,	/* 76 */
  -23,	/* 77 */
  -21,	/* 78 */
};

static const TDS_WORD limits[]= {
  0x00000001u,	/*   0 */
  0x0000000au,	/*   1 */
  0x00000064u,	/*   2 */
  0x000003e8u,	/*   3 */
  0x00002710u,	/*   4 */
  0x000186a0u,	/*   5 */
  0x000f4240u,	/*   6 */
  0x00989680u,	/*   7 */
  0x05f5e100u,	/*   8 */
  0x3b9aca00u,	/*   9 */
  0x00000002u,	/*  10 */
  0x540be400u,	/*  11 */
  0x00000017u,	/*  12 */
  0x4876e800u,	/*  13 */
  0x000000e8u,	/*  14 */
  0xd4a51000u,	/*  15 */
  0x00000918u,	/*  16 */
  0x4e72a000u,	/*  17 */
  0x00005af3u,	/*  18 */
  0x107a4000u,	/*  19 */
  0x00038d7eu,	/*  20 */
  0xa4c68000u,	/*  21 */
  0x002386f2u,	/*  22 */
  0x6fc10000u,	/*  23 */
  0x01634578u,	/*  24 */
  0x5d8a0000u,	/*  25 */
  0x0de0b6b3u,	/*  26 */
  0xa7640000u,	/*  27 */
  0x8ac72304u,	/*  28 */
  0x89e80000u,	/*  29 */
  0x00000005u,	/*  30 */
  0x6bc75e2du,	/*  31 */
  0x63100000u,	/*  32 */
  0x00000036u,	/*  33 */
  0x35c9adc5u,	/*  34 */
  0xdea00000u,	/*  35 */
  0x0000021eu,	/*  36 */
  0x19e0c9bau,	/*  37 */
  0xb2400000u,	/*  38 */
  0x0000152du,	/*  39 */
  0x02c7e14au,	/*  40 */
  0xf6800000u,	/*  41 */
  0x0000d3c2u,	/*  42 */
  0x1bceccedu,	/*  43 */
  0xa1000000u,	/*  44 */
  0x00084595u,	/*  45 */
  0x16140148u,	/*  46 */
  0x4a000000u,	/*  47 */
  0x0052b7d2u,	/*  48 */
  0xdcc80cd2u,	/*  49 */
  0xe4000000u,	/*  50 */
  0x033b2e3cu,	/*  51 */
  0x9fd0803cu,	/*  52 */
  0xe8000000u,	/*  53 */
  0x204fce5eu,	/*  54 */
  0x3e250261u,	/*  55 */
  0x10000000u,	/*  56 */
  0x00000001u,	/*  57 */
  0x431e0faeu,	/*  58 */
  0x6d7217cau,	/*  59 */
  0xa0000000u,	/*  60 */
  0x0000000cu,	/*  61 */
  0x9f2c9cd0u,	/*  62 */
  0x4674edeau,	/*  63 */
  0x40000000u,	/*  64 */
  0x0000007eu,	/*  65 */
  0x37be2022u,	/*  66 */
  0xc0914b26u,	/*  67 */
  0x80000000u,	/*  68 */
  0x000004eeu,	/*  69 */
  0x2d6d415bu,	/*  70 */
  0x85acef81u,	/*  71 */
  0x0000314du,	/*  72 */
  0xc6448d93u,	/*  73 */
  0x38c15b0au,	/*  74 */
  0x0001ed09u,	/*  75 */
  0xbead87c0u,	/*  76 */
  0x378d8e64u,	/*  77 */
  0x00134261u,	/*  78 */
  0x72c74d82u,	/*  79 */
  0x2b878fe8u,	/*  80 */
  0x00c097ceu,	/*  81 */
  0x7bc90715u,	/*  82 */
  0xb34b9f10u,	/*  83 */
  0x0785ee10u,	/*  84 */
  0xd5da46d9u,	/*  85 */
  0x00f436a0u,	/*  86 */
  0x4b3b4ca8u,	/*  87 */
  0x5a86c47au,	/*  88 */
  0x098a2240u,	/*  89 */
  0x00000002u,	/*  90 */
  0xf050fe93u,	/*  91 */
  0x8943acc4u,	/*  92 */
  0x5f655680u,	/*  93 */
  0x0000001du,	/*  94 */
  0x6329f1c3u,	/*  95 */
  0x5ca4bfabu,	/*  96 */
  0xb9f56100u,	/*  97 */
  0x00000125u,	/*  98 */
  0xdfa371a1u,	/*  99 */
  0x9e6f7cb5u,	/* 100 */
  0x4395ca00u,	/* 101 */
  0x00000b7au,	/* 102 */
  0xbc627050u,	/* 103 */
  0x305adf14u,	/* 104 */
  0xa3d9e400u,	/* 105 */
  0x000072cbu,	/* 106 */
  0x5bd86321u,	/* 107 */
  0xe38cb6ceu,	/* 108 */
  0x6682e800u,	/* 109 */
  0x00047bf1u,	/* 110 */
  0x9673df52u,	/* 111 */
  0xe37f2410u,	/* 112 */
  0x011d1000u,	/* 113 */
  0x002cd76fu,	/* 114 */
  0xe086b93cu,	/* 115 */
  0xe2f768a0u,	/* 116 */
  0x0b22a000u,	/* 117 */
  0x01c06a5eu,	/* 118 */
  0xc5433c60u,	/* 119 */
  0xddaa1640u,	/* 120 */
  0x6f5a4000u,	/* 121 */
  0x118427b3u,	/* 122 */
  0xb4a05bc8u,	/* 123 */
  0xa8a4de84u,	/* 124 */
  0x59868000u,	/* 125 */
  0xaf298d05u,	/* 126 */
  0x0e4395d6u,	/* 127 */
  0x9670b12bu,	/* 128 */
  0x7f410000u,	/* 129 */
  0x00000006u,	/* 130 */
  0xd79f8232u,	/* 131 */
  0x8ea3da61u,	/* 132 */
  0xe066ebb2u,	/* 133 */
  0xf88a0000u,	/* 134 */
  0x00000044u,	/* 135 */
  0x6c3b15f9u,	/* 136 */
  0x926687d2u,	/* 137 */
  0xc40534fdu,	/* 138 */
  0xb5640000u,	/* 139 */
  0x000002acu,	/* 140 */
  0x3a4edbbfu,	/* 141 */
  0xb8014e3bu,	/* 142 */
  0xa83411e9u,	/* 143 */
  0x15e80000u,	/* 144 */
  0x00001abau,	/* 145 */
  0x4714957du,	/* 146 */
  0x300d0e54u,	/* 147 */
  0x9208b31au,	/* 148 */
  0xdb100000u,	/* 149 */
  0x00010b46u,	/* 150 */
  0xc6cdd6e3u,	/* 151 */
  0xe0828f4du,	/* 152 */
  0xb456ff0cu,	/* 153 */
  0x8ea00000u,	/* 154 */
  0x000a70c3u,	/* 155 */
  0xc40a64e6u,	/* 156 */
  0xc5199909u,	/* 157 */
  0x0b65f67du,	/* 158 */
  0x92400000u,	/* 159 */
  0x006867a5u,	/* 160 */
  0xa867f103u,	/* 161 */
  0xb2fffa5au,	/* 162 */
  0x71fba0e7u,	/* 163 */
  0xb6800000u,	/* 164 */
  0x04140c78u,	/* 165 */
  0x940f6a24u,	/* 166 */
  0xfdffc788u,	/* 167 */
  0x73d4490du,	/* 168 */
  0x21000000u,	/* 169 */
  0x28c87cb5u,	/* 170 */
  0xc89a2571u,	/* 171 */
  0xebfdcb54u,	/* 172 */
  0x864ada83u,	/* 173 */
  0x4a000000u,	/* 174 */
  0x00000001u,	/* 175 */
  0x97d4df19u,	/* 176 */
  0xd6057673u,	/* 177 */
  0x37e9f14du,	/* 178 */
  0x3eec8920u,	/* 179 */
  0xe4000000u,	/* 180 */
  0x0000000fu,	/* 181 */
  0xee50b702u,	/* 182 */
  0x5c36a080u,	/* 183 */
  0x2f236d04u,	/* 184 */
  0x753d5b48u,	/* 185 */
  0xe8000000u,	/* 186 */
  0x0000009fu,	/* 187 */
  0x4f272617u,	/* 188 */
  0x9a224501u,	/* 189 */
  0xd762422cu,	/* 190 */
  0x946590d9u,	/* 191 */
  0x10000000u,	/* 192 */
  0x00000639u,	/* 193 */
  0x17877cecu,	/* 194 */
  0x0556b212u,	/* 195 */
  0x69d695bdu,	/* 196 */
  0xcbf7a87au,	/* 197 */
  0xa0000000u,	/* 198 */
  0x00003e3au,	/* 199 */
  0xeb4ae138u,	/* 200 */
  0x3562f4b8u,	/* 201 */
  0x2261d969u,	/* 202 */
  0xf7ac94cau,	/* 203 */
  0x40000000u,	/* 204 */
  0x00026e4du,	/* 205 */
  0x30eccc32u,	/* 206 */
  0x15dd8f31u,	/* 207 */
  0x57d27e23u,	/* 208 */
  0xacbdcfe6u,	/* 209 */
  0x80000000u,	/* 210 */
  0x00184f03u,	/* 211 */
  0xe93ff9f4u,	/* 212 */
  0xdaa797edu,	/* 213 */
  0x6e38ed64u,	/* 214 */
  0xbf6a1f01u,	/* 215 */
  0x00f31627u,	/* 216 */
  0x1c7fc390u,	/* 217 */
  0x8a8bef46u,	/* 218 */
  0x4e3945efu,	/* 219 */
  0x7a25360au,	/* 220 */
  0x097edd87u,	/* 221 */
  0x1cfda3a5u,	/* 222 */
  0x697758bfu,	/* 223 */
  0x0e3cbb5au,	/* 224 */
  0xc5741c64u,	/* 225 */
  0x5ef4a747u,	/* 226 */
  0x21e86476u,	/* 227 */
  0x1ea97776u,	/* 228 */
  0x8e5f518bu,	/* 229 */
  0xb6891be8u,	/* 230 */
  0x00000003u,	/* 231 */
  0xb58e88c7u,	/* 232 */
  0x5313ec9du,	/* 233 */
  0x329eaaa1u,	/* 234 */
  0x8fb92f75u,	/* 235 */
  0x215b1710u,	/* 236 */
  0x00000025u,	/* 237 */
  0x179157c9u,	/* 238 */
  0x3ec73e23u,	/* 239 */
  0xfa32aa4fu,	/* 240 */
  0x9d3bda93u,	/* 241 */
  0x4d8ee6a0u,	/* 242 */
  0x00000172u,	/* 243 */
  0xebad6ddcu,	/* 244 */
  0x73c86d67u,	/* 245 */
  0xc5faa71cu,	/* 246 */
  0x245689c1u,	/* 247 */
  0x07950240u,	/* 248 */
  0x00000e7du,	/* 249 */
  0x34c64a9cu,	/* 250 */
  0x85d4460du,	/* 251 */
  0xbbca8719u,	/* 252 */
  0x6b61618au,	/* 253 */
  0x4bd21680u,	/* 254 */
  0x000090e4u,	/* 255 */
  0x0fbeea1du,	/* 256 */
  0x3a4abc89u,	/* 257 */
  0x55e946feu,	/* 258 */
  0x31cdcf66u,	/* 259 */
  0xf634e100u,	/* 260 */
  0x0005a8e8u,	/* 261 */
  0x9d752524u,	/* 262 */
  0x46eb5d5du,	/* 263 */
  0x5b1cc5edu,	/* 264 */
  0xf20a1a05u,	/* 265 */
  0x9e10ca00u,	/* 266 */
  0x00389916u,	/* 267 */
  0x2693736au,	/* 268 */
  0xc531a5a5u,	/* 269 */
  0x8f1fbb4bu,	/* 270 */
  0x74650438u,	/* 271 */
  0x2ca7e400u,	/* 272 */
  0x0235faddu,	/* 273 */
  0x81c2822bu,	/* 274 */
  0xb3f07877u,	/* 275 */
  0x973d50f2u,	/* 276 */
  0x8bf22a31u,	/* 277 */
  0xbe8ee800u,	/* 278 */
  0x161bcca7u,	/* 279 */
  0x119915b5u,	/* 280 */
  0x0764b4abu,	/* 281 */
  0xe8652979u,	/* 282 */
  0x7775a5f1u,	/* 283 */
  0x71951000u,	/* 284 */
  0xdd15fe86u,	/* 285 */
  0xaffad912u,	/* 286 */
  0x49ef0eb7u,	/* 287 */
  0x13f39ebeu,	/* 288 */
  0xaa987b6eu,	/* 289 */
  0x6fd2a000u,	/* 290 */
};

static int
tds_packet_check_overflow(TDS_WORD *packet, unsigned int packet_len, unsigned int prec)
{
	unsigned int i, len, stop;
	const TDS_WORD *limit = &limits[limit_indexes[prec] + LIMIT_INDEXES_ADJUST * prec];
	len = limit_indexes[prec+1] - limit_indexes[prec] + LIMIT_INDEXES_ADJUST;
	stop = prec / (sizeof(TDS_WORD) * 8);
	/*
	 * Now a number is
	 * ... P[3] P[2] P[1] P[0]
	 * while upper limit + 1 is
 	 * zeroes limit[0 .. len-1] 0[0 .. stop-1]
	 * we must assure that number < upper limit + 1
	 */
	if (packet_len >= len + stop) {
		/* higher packets must be zero */
		for (i = packet_len; --i >= len + stop; )
			if (packet[i] > 0)
				return TDS_CONVERT_OVERFLOW;
		/* test limit */
		for (;; --i, ++limit) {
			if (i <= stop) {
				/* last must be >= not > */
				if (packet[i] >= *limit)
					return TDS_CONVERT_OVERFLOW;
				break;
			}
			if (packet[i] > *limit)
				return TDS_CONVERT_OVERFLOW;
			if (packet[i] < *limit)
				break;
		}
	}
	return 0;
}

TDS_INT
tds_numeric_change_prec_scale(TDS_NUMERIC * numeric, unsigned char new_prec, unsigned char new_scale)
{
	static const TDS_WORD factors[] = {
		1, 10, 100, 1000, 10000,
		100000, 1000000, 10000000, 100000000, 1000000000
	};

	TDS_WORD packet[(sizeof(numeric->array) - 1) / sizeof(TDS_WORD)];

	unsigned int i, packet_len;
	int scale_diff, bytes;

	if (numeric->precision < 1 || numeric->precision > MAXPRECISION || numeric->scale > numeric->precision)
		return TDS_CONVERT_FAIL;

	if (new_prec < 1 || new_prec > MAXPRECISION || new_scale > new_prec)
		return TDS_CONVERT_FAIL;

	scale_diff = new_scale - numeric->scale;
	if (scale_diff == 0 && new_prec >= numeric->precision) {
		i = tds_numeric_bytes_per_prec[new_prec] - tds_numeric_bytes_per_prec[numeric->precision];
		if (i > 0) {
			memmove(numeric->array + 1 + i, numeric->array + 1, sizeof(numeric->array) - 1 - i);
			memset(numeric->array + 1, 0, i);
		}
		numeric->precision = new_prec;
		return sizeof(TDS_NUMERIC);
	}

	/* package number */
	bytes = tds_numeric_bytes_per_prec[numeric->precision] - 1;
	i = 0;
	do {
		/*
		 * note that if bytes are smaller we have a small buffer
		 * overflow in numeric->array however is not a problem
		 * cause overflow occurs in numeric and number is fixed below
		 */
		packet[i] = TDS_GET_UA4BE(&numeric->array[bytes-3]);
		++i;
	} while ( (bytes -= sizeof(TDS_WORD)) > 0);
	/* fix last packet */
	if (bytes < 0)
		packet[i-1] &= 0xffffffffu >> (8 * -bytes);
	while (i > 1 && packet[i-1] == 0)
		--i;
	packet_len = i;

	if (scale_diff >= 0) {
		/* check overflow before multiply */
		if (tds_packet_check_overflow(packet, packet_len, new_prec - scale_diff))
			return TDS_CONVERT_OVERFLOW;

		if (scale_diff == 0) {
			i = tds_numeric_bytes_per_prec[numeric->precision] - tds_numeric_bytes_per_prec[new_prec];
			if (i > 0)
				memmove(numeric->array + 1, numeric->array + 1 + i, sizeof(numeric->array) - 1 - i);
			numeric->precision = new_prec;
			return sizeof(TDS_NUMERIC);
		}

		/* multiply */
		do {
			/* multiply by at maximun TDS_WORD_DDIGIT */
			unsigned int n = scale_diff > TDS_WORD_DDIGIT ? TDS_WORD_DDIGIT : scale_diff;
			TDS_WORD factor = factors[n];
			TDS_WORD carry = 0;
			scale_diff -= n; 
			for (i = 0; i < packet_len; ++i) {
				TDS_DWORD n = packet[i] * ((TDS_DWORD) factor) + carry;
				packet[i] = (TDS_WORD) n;
				carry = n >> (8 * sizeof(TDS_WORD));
			}
			/* here we can expand number safely cause we know that it can't overflow */
			if (carry)
				packet[packet_len++] = carry;
		} while (scale_diff > 0);
	} else {
		/* check overflow */
		if (new_prec - scale_diff < numeric->precision)
			if (tds_packet_check_overflow(packet, packet_len, new_prec - scale_diff))
				return TDS_CONVERT_OVERFLOW;

		/* divide */
		scale_diff = -scale_diff;
		do {
			unsigned int n = scale_diff > TDS_WORD_DDIGIT ? TDS_WORD_DDIGIT : scale_diff;
			TDS_WORD factor = factors[n];
			TDS_WORD borrow = 0;
			scale_diff -= n;
			for (i = packet_len; i > 0; ) {
#if defined(__GNUC__) && __GNUC__ >= 3 && defined(__i386__)
				--i;
				__asm__ ("divl %4": "=a"(packet[i]), "=d"(borrow): "0"(packet[i]), "1"(borrow), "r"(factor));
#elif defined(__WATCOMC__) && defined(DOS32X)
				TDS_WORD Int64div32(TDS_WORD* low,TDS_WORD high,TDS_WORD factor);
				#pragma aux Int64div32 = "mov eax, dword ptr[esi]" \
					"div ecx" \
					"mov dword ptr[esi], eax" \
					parm [ESI] [EDX] [ECX] value [EDX] modify [EAX EDX];
				borrow = Int64div32(&packet[i], borrow, factor);
#else
				TDS_DWORD n = (((TDS_DWORD) borrow) << (8 * sizeof(TDS_WORD))) + packet[--i];
				packet[i] = (TDS_WORD) (n / factor);
				borrow = n % factor;
#endif
			}
		} while (scale_diff > 0);
	}

	/* back to our format */
	numeric->precision = new_prec;
	numeric->scale = new_scale;
	bytes = tds_numeric_bytes_per_prec[numeric->precision] - 1;
	for (i = bytes / sizeof(TDS_WORD); i >= packet_len; --i)
		packet[i] = 0;
	for (i = 0; bytes >= sizeof(TDS_WORD); bytes -= sizeof(TDS_WORD), ++i) {
		TDS_PUT_UA4BE(&numeric->array[bytes-3], packet[i]);
	}

	if (bytes) {
		TDS_WORD remainder = packet[i];
		do {
			numeric->array[bytes] = (TDS_UCHAR) remainder;
			remainder >>= 8;
		} while (--bytes);
	}

	return sizeof(TDS_NUMERIC);
}

