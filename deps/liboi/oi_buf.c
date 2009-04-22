#include <oi_buf.h>
#include <stdlib.h>
#include <string.h>

void oi_buf_destroy 
  ( oi_buf *buf
  )
{
  free(buf->base);
  free(buf);
}

oi_buf * oi_buf_new2
  ( size_t len
  )
{
  oi_buf *buf = malloc(sizeof(oi_buf));
  if(!buf) 
    return NULL;
  buf->base = malloc(len);
  if(!buf->base) {
    free(buf);
    return NULL; 
  }
  buf->len = len;
  buf->release = oi_buf_destroy;
  return buf;
}

oi_buf * oi_buf_new
  ( const char *base
  , size_t len
  )
{
  oi_buf *buf = oi_buf_new2(len);
  if(!buf) 
    return NULL;
  memcpy(buf->base, base, len);
  return buf;
}

