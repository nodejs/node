#include <assert.h>
#include <sys/resource.h>

int main() {
  struct rusage ru1;
  struct rusage ru2;
  long long s1;
  long long us1;
  long long s2;
  long long us2;
  int r;
  int success = 0;

  r = getrusage(RUSAGE_SELF, &ru1);
  assert(r == 0);
  s1 = ru1.ru_utime.tv_sec;
  us1 = ru1.ru_utime.tv_usec;

  for (int i = 0; i < 10000; i++) {
    r = getrusage(RUSAGE_SELF, &ru2);
    assert(r == 0);
    s2 = ru2.ru_utime.tv_sec;
    us2 = ru2.ru_utime.tv_usec;
    assert(s1 <= s2);

    // Verify that some time has passed.
    if (s2 > s1 || (s2 == s1 && us2 > us1)) {
      success = 1;
      break;
    }
  }

  assert(success == 1);
}
