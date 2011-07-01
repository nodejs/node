/* This header is private to libuv */
#ifndef UV_EIO_H_
#define UV_EIO_H_

#include "eio.h"

/*
 * Call this function to integrate libeio into the libuv event loop. It is
 * safe to call more than once.
 * TODO: uv_eio_deinit
 */
void uv_eio_init(void);
#endif
