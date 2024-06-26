/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 *
 * Copyright (C) 2005 Liam Widdowson
 * Copyright (C) 2010-2012 Frediano Ziglio
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

#ifndef TDSTHREAD_H
#define TDSTHREAD_H 1

#include <tds_sysdep_public.h>
#include <pthread.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef pthread_mutex_t tds_raw_mutex;
#define TDS_RAW_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline void tds_raw_mutex_lock(tds_raw_mutex *mtx) {
  pthread_mutex_lock(mtx);
}

static inline int tds_raw_mutex_trylock(tds_raw_mutex *mtx) {
  return pthread_mutex_trylock(mtx);
}

static inline void tds_raw_mutex_unlock(tds_raw_mutex *mtx) {
  pthread_mutex_unlock(mtx);
}

static inline int tds_raw_mutex_init(tds_raw_mutex *mtx) {
  return pthread_mutex_init(mtx, NULL);
}

static inline void tds_raw_mutex_free(tds_raw_mutex *mtx) {
  pthread_mutex_destroy(mtx);
}

typedef pthread_cond_t tds_condition;

static inline int tds_raw_cond_destroy(tds_condition *cond) {
  return pthread_cond_destroy(cond);
}

static inline int tds_raw_cond_signal(tds_condition *cond) {
  return pthread_cond_signal(cond);
}

static inline int tds_raw_cond_wait(tds_condition *cond, tds_raw_mutex *mtx) {
  return pthread_cond_wait(cond, mtx);
}

#define TDS_HAVE_MUTEX 1

typedef pthread_t tds_thread;
typedef pthread_t tds_thread_id;

typedef void *(*tds_thread_proc)(void *arg);

#define TDS_THREAD_PROC_DECLARE(name, arg) \
  void *name(void *arg)
#define TDS_THREAD_RESULT(n) ((void*)(intptr_t)(n))

static inline int tds_thread_create(tds_thread *ret, tds_thread_proc proc, void *arg) {
  return pthread_create(ret, NULL, proc, arg);
}

static inline int tds_thread_create_detached(tds_thread_proc proc, void *arg) {
  tds_thread th;
  int ret = pthread_create(&th, NULL, proc, arg);
  if (!ret)
    pthread_detach(th);
  return ret;
}

static inline int tds_thread_join(tds_thread th, void **ret) {
  return pthread_join(th, ret);
}

static inline tds_thread_id tds_thread_get_current_id(void) {
  return pthread_self();
}

static inline int tds_thread_is_current(tds_thread_id th) {
  return pthread_equal(th, pthread_self());
}

#  define tds_cond_destroy tds_raw_cond_destroy
#  define tds_cond_signal tds_raw_cond_signal
#    define TDS_MUTEX_INITIALIZER TDS_RAW_MUTEX_INITIALIZER
#    define tds_mutex tds_raw_mutex
#    define tds_mutex_lock tds_raw_mutex_lock
#    define tds_mutex_trylock tds_raw_mutex_trylock
#    define tds_mutex_unlock tds_raw_mutex_unlock
#    define tds_mutex_check_owned(mtx) do {} while(0)
#    define tds_mutex_init tds_raw_mutex_init
#    define tds_mutex_free tds_raw_mutex_free
#    define tds_cond_wait tds_raw_cond_wait

#ifdef __cplusplus
}
#endif


#endif
