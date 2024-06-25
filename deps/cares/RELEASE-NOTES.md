## c-ares version 1.31.0 - June 18 2024

This is a maintenance and bugfix release.

Changes:

* Enable Query Cache by default. [PR #786](https://github.com/c-ares/c-ares/pull/786)

Bugfixes:

* Enhance Windows DNS configuration change detection to also detect manual DNS
  configuration changes. [PR #785](https://github.com/c-ares/c-ares/issues/785)
* Various legacy MacOS Build fixes. [Issue #782](https://github.com/c-ares/c-ares/issues/782)
* Ndots value of zero in resolv.conf was not being honored. [852a60a](https://github.com/c-ares/c-ares/commit/852a60a)
* Watt-32 build support had been broken for some time. [PR #781](https://github.com/c-ares/c-ares/pull/781)
* Distribute `ares_dns_rec_type_tostr` manpage. [PR #778](https://github.com/c-ares/c-ares/pull/778)

Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)
* Gregor Jasny (@gjasny)


