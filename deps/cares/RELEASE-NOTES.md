## c-ares version 1.34.6 - December 8 2025

This is a security release.

Security:
* CVE-2025-62408. A use-after-free bug has been uncovered in read_answers() that
  was introduced in v1.32.3. Please see https://github.com/c-ares/c-ares/security/advisories/GHSA-jq53-42q6-pqr5

Changes:
* Ignore Windows IDN Search Domains until proper IDN support is added. [PR #1034](https://github.com/c-ares/c-ares/pull/1034)

Bugfixes:
* Event Thread could stall when not notified of new queries on existing
  connections that are in a bad state
  [PR #1032](https://github.com/c-ares/c-ares/pull/1032)
* fix conversion of invalid service to port number in ares_getaddrinfo()
  [PR #1029](https://github.com/c-ares/c-ares/pull/1029)
* fix memory leak in ares_uri
  [PR #1012](https://github.com/c-ares/c-ares/pull/1012)
* Ignore ares_event_configchg_init failures
  [PR #1009](https://github.com/c-ares/c-ares/pull/1009)
* Use XOR for random seed generation on fallback logic.
  [PR #994](https://github.com/c-ares/c-ares/pull/994)
* Fix clang build on windows.
  [PR #996](https://github.com/c-ares/c-ares/pull/996)
* Fix IPv6 link-local nameservers in /etc/resolv.conf
  [PR #996](https://github.com/c-ares/c-ares/pull/997)
* Fix a few build issues on MidnightBSD.
  [PR #983](https://github.com/c-ares/c-ares/pull/983)

Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)
* (@F3lixTheCat)
* Lucas Holt (@laffer1)
* @oargon
* Pavel P (@pps83)
* Sean Harmer (@seanharmer)
* Uwe (@nixblik)


