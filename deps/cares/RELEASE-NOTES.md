## c-ares version 1.33.1 - August 23 2024

This is a bugfix release.

Bugfixes:
* Work around systemd-resolved quirk that returns unexpected codes for single
  label names.  Also adds test cases to validate the work around works and
  will continue to work in future releases.
  [PR #863](https://github.com/c-ares/c-ares/pull/863),
  See Also https://github.com/systemd/systemd/issues/34101
* Fix sysconfig ndots default value, also adds containerized test case to
  prevent future regressions.
  [PR #862](https://github.com/c-ares/c-ares/pull/862)
* Fix blank DNS name returning error code rather than valid record for
  commands like: `adig -t SOA .`.  Also adds test case to prevent future
  regressions.
  [9e574af](https://github.com/c-ares/c-ares/commit/9e574af)
* Fix calculation of query times > 1s.
  [2b2eae7](https://github.com/c-ares/c-ares/commit/2b2eae7)
* Fix building on old Linux releases that don't have `TCP_FASTOPEN_CONNECT`.
  [b7a89b9](https://github.com/c-ares/c-ares/commit/b7a89b9)
* Fix minor Android build warnings.
  [PR #848](https://github.com/c-ares/c-ares/pull/848)

Thanks go to these friendly people for their efforts and contributions for this
release:
* Brad House (@bradh352)
* Erik Lax (@eriklax)
* Hans-Christian Egtvedt (@egtvedt)
* Mikael Lindemann (@mikaellindemann)
* Nodar Chkuaselidze (@nodech)

## c-ares version 1.33.0 - August 2 2024

This is a feature and bugfix release.

Features:
* Add DNS cookie support (RFC7873 + RFC9018) to help prevent off-path cache
  poisoning attacks. [PR #833](https://github.com/c-ares/c-ares/pull/833)
* Implement TCP FastOpen (TFO) RFC7413, which will make TCP reconnects 0-RTT
  on supported systems. [PR #840](https://github.com/c-ares/c-ares/pull/840)

Changes:
* Reorganize source tree. [PR #822](https://github.com/c-ares/c-ares/pull/822)
* Refactoring of connection handling to prevent code duplication.
  [PR #839](https://github.com/c-ares/c-ares/pull/839)
* New dynamic array data structure to prevent simple logic flaws in array
  handling in various code paths.
  [PR #841](https://github.com/c-ares/c-ares/pull/841)

Bugfixes:
* `ares_destroy()` race condition during shutdown due to missing lock.
  [PR #831](https://github.com/c-ares/c-ares/pull/831)
* Android: Preserve thread name after attaching it to JVM.
  [PR #838](https://github.com/c-ares/c-ares/pull/838)
* Windows UWP (Store) support fix.
  [PR #845](https://github.com/c-ares/c-ares/pull/845)


Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)
* Yauheni Khnykin (@Hsilgos)

