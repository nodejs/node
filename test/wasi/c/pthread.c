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
  const int max_retries = 5;

  // Retry pthread_create if it fails due to resource exhaustion
  do {
    r = pthread_create(&thread, NULL, worker, &result);
    if (r == 0) {
      break; // Success
    }
    
    // If it's a resource issue (EAGAIN/ENOMEM), retry with a small delay
    if ((r == EAGAIN || r == ENOMEM) && retry_count < max_retries) {
      retry_count++;
      usleep(100000 * retry_count); // Exponential backoff: 100ms, 200ms, etc.
    } else {
      // Non-recoverable error or max retries reached
      assert(r == 0);
    }
  } while (r != 0 && retry_count <= max_retries);

  r = pthread_join(thread, NULL);
  assert(r == 0);

  assert(result == 42);
  return 0;
}
