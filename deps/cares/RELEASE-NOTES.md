## c-ares version 1.30.0 - June 7 2024

This is a maintenance and bugfix release.

Features:

* Basic support for SIG RR record (RFC 2931 / RFC 2535) [PR #773](https://github.com/c-ares/c-ares/pull/773)

Changes:

* Validation that DNS strings can only consist of printable ascii characters
  otherwise will trigger a parse failure.
  [75de16c](https://github.com/c-ares/c-ares/commit/75de16c) and
  [40fb125](https://github.com/c-ares/c-ares/commit/40fb125)
* Windows: use `GetTickCount64()` for a monotonic timer that does not wrap. [1dff8f6](https://github.com/c-ares/c-ares/commit/1dff8f6)

Bugfixes:

* QueryCache: Fix issue where purging on server changes wasn't working. [a6c8fe6](https://github.com/c-ares/c-ares/commit/a6c8fe6)
* Windows: Fix Y2K38 issue by creating our own `ares_timeval_t` datatype. [PR #772](https://github.com/c-ares/c-ares/pull/772)
* Fix packaging issue affecting MacOS due to a missing header. [55afad6](https://github.com/c-ares/c-ares/commit/55afad6)
* MacOS: Fix UBSAN warnings that are likely meaningless due to alignment issues
  in new MacOS config reader.
* Android: arm 32bit build failure due to missing symbol. [d1722e6](https://github.com/c-ares/c-ares/commit/d1722e6)

Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)
* Daniel Stenberg (@bagder)



