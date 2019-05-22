#include <assert.h>
#include <sys/resource.h>

int main() {
  struct rusage ru1;
  getrusage(RUSAGE_SELF, &ru1);

  for (int i = 0; i < 1000; i++) {
  }

  struct rusage ru2;
  getrusage(RUSAGE_SELF, &ru2);

  // assert that some time has passed
  long long s1 = ru1.ru_utime.tv_sec;
  long long us1 = ru1.ru_utime.tv_usec;
  long long s2 = ru2.ru_utime.tv_sec;
  long long us2 = ru2.ru_utime.tv_usec;
  assert(s1 <= s2);
  if (s1 == s2) {
    // strictly less than, so the timestamps can't be equal
    assert(us1 < us2);
  }
}
