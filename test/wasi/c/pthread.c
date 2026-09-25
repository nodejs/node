#include <assert.h>
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

  int r = pthread_create(&thread, NULL, worker, &result);
  assert(r == 0);

  r = pthread_join(thread, NULL);
  assert(r == 0);

  assert(result == 42);
  return 0;
}
