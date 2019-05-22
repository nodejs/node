#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>

int main() {
  struct timeval tv1;
  gettimeofday(&tv1, NULL);

  for (int i = 0; i < 1000; i++) {
  }

  struct timeval tv2;
  gettimeofday(&tv2, NULL);

  // assert that some time has passed
  long long s1 = tv1.tv_sec;
  long long us1 = tv1.tv_usec;
  long long s2 = tv2.tv_sec;
  long long us2 = tv2.tv_usec;
  assert(s1 <= s2);
  if (s1 == s2) {
    // strictly less than, so the timestamps can't be equal
    assert(us1 < us2);
  }
}
