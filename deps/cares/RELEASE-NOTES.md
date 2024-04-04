## c-ares version 1.28.1 - Mar 30 2024

This release contains a fix for a single significant regression introduced
in c-ares 1.28.0.

* `ares_search()` and `ares_getaddrinfo()` resolution fails if no search domains
  are specified. [Issue #737](https://github.com/c-ares/c-ares/issues/737)


## c-ares version 1.28.0 - Mar 29 2024

This is a feature and bugfix release.

Features:

* Emit warnings when deprecated c-ares functions are used.  This can be
  disabled by passing a compiler definition of `CARES_NO_DEPRECATED`. [PR #732](https://github.com/c-ares/c-ares/pull/732)
* Add function `ares_search_dnsrec()` to search for records using the new DNS
  record data structures. [PR #719](https://github.com/c-ares/c-ares/pull/719)
* Rework internals to pass around `ares_dns_record_t` instead of binary data,
  this introduces new public functions of `ares_query_dnsrec()` and
  `ares_send_dnsrec()`. [PR #730](https://github.com/c-ares/c-ares/pull/730)

Changes:

* tests: when performing simulated queries, reduce timeouts to make tests run
  faster
* Replace configuration file parsers with memory-safe parser. [PR #725](https://github.com/c-ares/c-ares/pull/725)
* Remove `acountry` completely, the manpage might still get installed otherwise. [Issue #718](https://github.com/c-ares/c-ares/pull/718)

Bugfixes:

* CMake: don't overwrite global required libraries/definitions/includes which
  could cause build errors for projects chain building c-ares. [Issue #729](https://github.com/c-ares/c-ares/issues/729)
* On some platforms, `netinet6/in6.h` is not included by `netinet/in.h`
  and needs to be included separately. [PR #728](https://github.com/c-ares/c-ares/pull/728)
* Fix a potential memory leak in `ares_init()`. [Issue #724](https://github.com/c-ares/c-ares/issues/724)
* Some platforms don't have the `isascii()` function.  Implement as a macro. [PR #721](https://github.com/c-ares/c-ares/pull/721)
* CMake: Fix Chain building if CMAKE runtime paths not set
* NDots configuration should allow a value of zero. [PR #735](https://github.com/c-ares/c-ares/pull/735)

Thanks go to these friendly people for their efforts and contributions for this release:

* Brad House (@bradh352)
* Cristian Rodríguez (@crrodriguez)
* Daniel Stenberg (@bagder)
* Faraz (@farazrbx)
* Faraz Fallahi (@fffaraz)
* Oliver Welsh (@oliverwelsh)
