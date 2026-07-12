#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>

int main() {
  struct timeval tv1;
  struct timeval tv2;
  long long s1;
  long long us1;
  long long s2;
  long long us2;
  int r;
  int success = 0;

  r = gettimeofday(&tv1, NULL);
  assert(r == 0);
  s1 = tv1.tv_sec;
  us1 = tv1.tv_usec;

  for (int i = 0; i < 10000; i++) {
    r = gettimeofday(&tv2, NULL);
    assert(r == 0);
    s2 = tv2.tv_sec;
    us2 = tv2.tv_usec;
    assert(s1 <= s2);

    // Verify that some time has passed.
    if (s2 > s1 || (s2 == s1 && us2 > us1)) {
      success = 1;
      break;
    }
  }

  assert(success == 1);
}
