## c-ares version 1.26.0 - Jan 26 2024

This is a feature and bugfix release.

Features:

* Event Thread support.  Integrators are no longer requried to monitor the
  file descriptors registered by c-ares for events and call `ares_process()`
  when enabling the event thread feature via `ARES_OPT_EVENT_THREAD` passed
  to `ares_init_options()`. [PR #696](https://github.com/c-ares/c-ares/pull/696)
* Added flags to `are_dns_parse()` to force RAW packet parsing.
  [PR #693](https://github.com/c-ares/c-ares/pull/693)

Changes:

* Mark `ares_fds()` as deprected.
  [PR #691](https://github.com/c-ares/c-ares/pull/691)

Bugfixes:

* `adig`: Differentiate between internal and server errors.
  [e10b16a](https://github.com/c-ares/c-ares/commit/e10b16a)
* Autotools allow make to override CFLAGS/CPPFLAGS/CXXFLAGS.
  [PR #695](https://github.com/c-ares/c-ares/pull/695)
* Autotools: fix building for 32bit windows due to stdcall symbol mangling.
  [PR #689](https://github.com/c-ares/c-ares/pull/689)
* RR Name should not be sanity checked against the Question.
  [PR #685](https://github.com/c-ares/c-ares/pull/685)

Thanks go to these friendly people for their efforts and contributions for this release:

* Brad House (@bradh352)
* Daniel Stenberg (@bagder)
* Erik Lax (@eriklax)
* Gisle Vanem (@gvanem)



