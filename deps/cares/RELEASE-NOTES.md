## c-ares version 1.29.0 - May 24 2024

This is a feature and bugfix release.

Features:

* When using `ARES_OPT_EVENT_THREAD`, automatically reload system configuration
  when network conditions change. [PR #759](https://github.com/c-ares/c-ares/pull/759)
* Apple: reimplement DNS configuration reading to more accurately pull DNS
  settings. [PR #750](https://github.com/c-ares/c-ares/pull/750)
* Add observability into DNS server health via a server state callback, invoked
  whenever a query finishes. [PR #744](https://github.com/c-ares/c-ares/pull/744)
* Add server failover retry behavior, where failed servers are retried with
  small probability after a minimum delay. [PR #731](https://github.com/c-ares/c-ares/pull/731)

Changes:

* Mark `ares_channel_t *` as const in more places in the public API. [PR #758](https://github.com/c-ares/c-ares/pull/758)

Bugfixes:

* Due to a logic flaw dns name compression writing was not properly implemented
  which would result in the name prefix not being written for a partial match.
  This could cause issues in various record types such as MX records when using
  the deprecated API.  Regression introduced in 1.28.0. [Issue #757](https://github.com/c-ares/c-ares/issues/757)
* Revert OpenBSD `SOCK_DNS` flag, it doesn't do what the docs say it does and
  causes c-ares to become non-functional. [PR #754](https://github.com/c-ares/c-ares/pull/754)
* `ares_getnameinfo()`: loosen validation on `salen` parameter. [Issue #752](https://github.com/c-ares/c-ares/issues/752)
* cmake: Android requires C99. [PR #748](https://github.com/c-ares/c-ares/pull/748)
* `ares_queue_wait_empty()` does not honor timeout_ms >= 0. [Issue #742](https://github.com/c-ares/c-ares/pull/742)

Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)
* Daniel Stenberg (@bagder)
* David Hotham (@dimbleby)
* Jiwoo Park (@jimmy-park)
* Oliver Welsh (@oliverwelsh)
* Volker Schlecht (@VlkrS)


