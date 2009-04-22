#include <oi_queue.h>

#ifndef oi_buf_h
#define oi_buf_h
#ifdef __cplusplus
extern "C" {
#endif 

typedef struct oi_buf oi_buf;

struct oi_buf {
  /* public */
  char *base;
  size_t len;
  void (*release) (oi_buf *); /* called when oi is done with the object */
  void *data;

  /* private */
  size_t written;
  oi_queue queue;
};

oi_buf * oi_buf_new     (const char* base, size_t len);
oi_buf * oi_buf_new2    (size_t len);
void     oi_buf_destroy (oi_buf *);

#ifdef __cplusplus
}
#endif 
#endif // oi_buf_h
