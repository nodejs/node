#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

void* worker(void* data) {
  int* result = (int*) data;
  sleep(1);
  *result = 42;
  return NULL;
}

int main() {
  pthread_t thread = NULL;
  int result = 0;
  int r;
  int retry_count = 0;
  const int max_retries = 3;

  // Retry pthread_create if it fails due to resource exhaustion
  for (retry_count = 0; retry_count <= max_retries; retry_count++) {
    r = pthread_create(&thread, NULL, worker, &result);
    if (r == 0) {
      break; // Success
    }
    
    // If it's a resource issue (EAGAIN/ENOMEM), retry with a small delay
    if ((r == EAGAIN || r == ENOMEM) && retry_count < max_retries) {
      // Exponential backoff: 50ms, 100ms, 200ms
      usleep(50000 * (1 << retry_count));
    } else {
      // Non-recoverable error or max retries reached
      break;
    }
  }
  
  // Assert after all retries are exhausted
  assert(r == 0);

  r = pthread_join(thread, NULL);
  assert(r == 0);

  assert(result == 42);
  return 0;
}
