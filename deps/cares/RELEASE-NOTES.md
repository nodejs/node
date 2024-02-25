## c-ares version 1.27.0 - Feb 23 2024

This is a security, feature, and bugfix release.

Security:

* Moderate. CVE-2024-25629. Reading malformatted `/etc/resolv.conf`,
  `/etc/nsswitch.conf` or the `HOSTALIASES` file could result in a crash.
  [GHSA-mg26-v6qh-x48q](https://github.com/c-ares/c-ares/security/advisories/GHSA-mg26-v6qh-x48q)

Features:

* New function `ares_queue_active_queries()` to retrieve number of in-flight
  queries. [PR #712](https://github.com/c-ares/c-ares/pull/712)
* New function `ares_queue_wait_empty()` to wait for the number of in-flight
  queries to reach zero. [PR #710](https://github.com/c-ares/c-ares/pull/710)
* New `ARES_FLAG_NO_DEFLT_SVR` for `ares_init_options()` to return a failure if
  no DNS servers can be found rather than attempting to use `127.0.0.1`. This
  also introduces a new ares status code of `ARES_ENOSERVER`. [PR #713](https://github.com/c-ares/c-ares/pull/713)

Changes:

* EDNS Packet size should be 1232 as per DNS Flag Day. [PR #705](https://github.com/c-ares/c-ares/pull/705)

Bugfixes:

* Windows DNS suffix search list memory leak. [PR #711](https://github.com/c-ares/c-ares/pull/711)
* Fix warning due to ignoring return code of `write()`. [PR #709](https://github.com/c-ares/c-ares/pull/709)
* CMake: don't override target output locations if not top-level. [Issue #708](https://github.com/c-ares/c-ares/issues/708)
* Fix building c-ares without thread support. [PR #700](https://github.com/c-ares/c-ares/pull/700)

Thanks go to these friendly people for their efforts and contributions for this release:

* Anthony Alayo (@anthonyalayo)
* Brad House (@bradh352)
* Cheng Zhao (@zcbenz)
* Cristian Rodríguez (@crrodriguez)
* Daniel Stenberg (@bagder)
* Oliver Welsh (@oliverwelsh)
* Vojtěch Vobr (@vojtechvobr)
