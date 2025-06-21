## c-ares version 1.34.5 - April 8 2025

This is a security release.

Security:
* CVE-2025-31498. A use-after-free bug has been uncovered in read_answers() that
  was introduced in v1.32.3.  Please see https://github.com/c-ares/c-ares/security/advisories/GHSA-6hxc-62jh-p29v

Changes:
* Restore Windows XP support. [PR #958](https://github.com/c-ares/c-ares/pull/958)

Bugfixes:
* A missing mutex initialization would make busy polling for configuration
  changes (platforms other than Windows, Linux, MacOS) eat too much CPU
  [PR #974](https://github.com/c-ares/c-ares/pull/974)
* Pkgconfig may be generated wrong for static builds in relation to `-pthread`
  [PR #965](https://github.com/c-ares/c-ares/pull/965)
* Localhost resolution can fail if only one address family is in `/etc/hosts`
  [PR #947](https://github.com/c-ares/c-ares/pull/947)

Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)
* Erik Lax (@eriklax)
* Florian Pfisterer (@FlorianPfisterer)
* Kai Pastor (@dg0yt)

