## c-ares version 1.34.8 - July 7 2026

This is a bugfix release.

Bugfixes:
* Revert "Mark parameters in callbacks as const" which shipped in 1.34.7.
  Changing the parameter types of the `ares_callback`, `ares_host_callback`,
  and `ares_nameinfo_callback` function pointer typedefs was an unintended
  API break: existing applications with the historical non-const callback
  signatures no longer compiled, particularly in C++ where function pointer
  types must match exactly. The read-only nature of the callback parameters
  is now documented instead.
  [PR #1244](https://github.com/c-ares/c-ares/pull/1244)

Thanks go to these friendly people for their efforts and contributions for this
release:

* Brad House (@bradh352)


## c-ares version 1.34.7 - July 6 2026

This is a security release.

Security:
* CVE-2026-33630. Use-after-free / double-free in c-ares' query-completion
  handling, remotely triggerable via ares_getaddrinfo() over TCP. Please see
  https://github.com/c-ares/c-ares/security/advisories/GHSA-6wfj-rwm7-3542
* CPU-exhaustion denial of service via unbounded DNS name compression pointer
  chains. Please see
  https://github.com/c-ares/c-ares/security/advisories/GHSA-pjmc-gx33-gc76
* Memory-amplification denial of service via unvalidated DNS header record
  counts. Please see
  https://github.com/c-ares/c-ares/security/advisories/GHSA-jv8r-gqr9-68wj

Changes:
* `ares_getaddrinfo()`: handle a NULL node per POSIX and implement
  `ARES_AI_PASSIVE`. [PR #1186](https://github.com/c-ares/c-ares/pull/1186)
* Mark parameters in callbacks as const.
  [PR #1060](https://github.com/c-ares/c-ares/pull/1060)
* Correct `ARES_CLASS_HESOID` misspelling to `ARES_CLASS_HESIOD`.
  [PR #1092](https://github.com/c-ares/c-ares/pull/1092)

Bugfixes:
* Fix sticky server recovery when all servers have failures. [PR #1192](https://github.com/c-ares/c-ares/pull/1192)
* Fix UDP socket exhaustion regression: retire connections per-connection, not via server failure count. [PR #1197](https://github.com/c-ares/c-ares/pull/1197)
* Guard DNS record binary length overflow. [PR #1168](https://github.com/c-ares/c-ares/pull/1168)
* Prevent integer overflow in allocation size calculations. [PR #1147](https://github.com/c-ares/c-ares/pull/1147)
* Prevent overflow in ares_array allocation size calculations. [PR #1117](https://github.com/c-ares/c-ares/pull/1117)
* Prevent integer overflow in buffer size calculation. [PR #1116](https://github.com/c-ares/c-ares/pull/1116)
* Add overflow checks to ares_buf_ensure_space(). [PR #1094](https://github.com/c-ares/c-ares/pull/1094)
* Skip name compression offsets beyond the 14-bit pointer limit. [PR #1159](https://github.com/c-ares/c-ares/pull/1159)
* ares_dns_parse: reject name compression in RDATA where not permitted (RFC 3597). [PR #1190](https://github.com/c-ares/c-ares/pull/1190)
* ares_dns_parse: reject responses with more than one OPT record (RFC 6891). [PR #1189](https://github.com/c-ares/c-ares/pull/1189)
* Discard oversized UDP datagrams instead of truncating the length frame. [PR #1161](https://github.com/c-ares/c-ares/pull/1161)
* Route numeric config parsing through range-checked ares_str_parse_uint. [PR #1158](https://github.com/c-ares/c-ares/pull/1158)
* Replace atoi-based port parsing with validated helper. [PR #1097](https://github.com/c-ares/c-ares/pull/1097)
* Defer TCP connection error handling until DNS responses are parsed. [PR #1138](https://github.com/c-ares/c-ares/pull/1138)
* Prevent undefined-behavior left shift in ares_calc_query_timeout(). [PR #1151](https://github.com/c-ares/c-ares/pull/1151)
* Use unsigned type for ares_round_up_pow2_u64 to avoid undefined behavior. [PR #1107](https://github.com/c-ares/c-ares/pull/1107)
* Fix undefined behavior in ares_buf_replace() pointer arithmetic. [PR #1099](https://github.com/c-ares/c-ares/pull/1099)
* ares_array: reset offset when array becomes empty. [PR #1165](https://github.com/c-ares/c-ares/pull/1165)
* ares_iface_ips: add ARES_IFACE_IP_NONE zero enum value. [PR #1187](https://github.com/c-ares/c-ares/pull/1187)
* ares_sysconfig_files: recognize AIX netsvc.conf bind4/local4 tokens. [PR #1188](https://github.com/c-ares/c-ares/pull/1188)
* Use safe string construction in Windows sysconfig join path. [PR #1143](https://github.com/c-ares/c-ares/pull/1143)
* Fix zero-length RAW_RR losing type metadata during parsing. [PR #1129](https://github.com/c-ares/c-ares/pull/1129)
* Fix RAW_RR type tostr/fromstr roundtrip mismatch. [PR #1123](https://github.com/c-ares/c-ares/pull/1123)
* Fix NULL dereference for ifa netmask. [PR #1120](https://github.com/c-ares/c-ares/pull/1120)
* adig: fix negated option prefix parsing. [PR #1135](https://github.com/c-ares/c-ares/pull/1135)
* Use UnregisterWaitEx to prevent use-after-free on Win32. [PR #1111](https://github.com/c-ares/c-ares/pull/1111)
* Add NULL check after ares_malloc_zero in Windows UTF8 conversion. [PR #1121](https://github.com/c-ares/c-ares/pull/1121)
* Fix NULL dereference after ares_malloc_zero in Win32 IOCP event add. [PR #1132](https://github.com/c-ares/c-ares/pull/1132)
* Fix microsecond overflow in ares_queue_wait_empty timeout. [PR #1122](https://github.com/c-ares/c-ares/pull/1122)
* Fix NULL dereference in ares_buf_replace() on NULL buf. [PR #1124](https://github.com/c-ares/c-ares/pull/1124)
* Initialize *read_bytes in ares_socket_recvfrom(). [PR #1125](https://github.com/c-ares/c-ares/pull/1125)
* Fix memory leak of binbuf in ares_buf_parse_dns_binstr_int. [PR #1126](https://github.com/c-ares/c-ares/pull/1126)
* Fix memory leak of qcache entry on key allocation failure. [PR #1127](https://github.com/c-ares/c-ares/pull/1127)
* Fix memory leak of buf in ares_dns_multistring_combined() on OOM. [PR #1110](https://github.com/c-ares/c-ares/pull/1110)
* Fix memory leaks on error paths in two functions. [PR #1109](https://github.com/c-ares/c-ares/pull/1109)
* Fix memory leak of buckets in ares_htable_dict_keys() error path. [PR #1108](https://github.com/c-ares/c-ares/pull/1108)
* Fix memory leak of bucket->key in ares_htable_dict_insert error path. [PR #1105](https://github.com/c-ares/c-ares/pull/1105)
* Fix additional memory leaks. [PR #1091](https://github.com/c-ares/c-ares/pull/1091) [PR #1078](https://github.com/c-ares/c-ares/pull/1078)
* Prevent corrupt addrinfo nodes on sockaddr allocation failure. [PR #1112](https://github.com/c-ares/c-ares/pull/1112)
* Fix two logic bugs in DNS cookie handling. [PR #1103](https://github.com/c-ares/c-ares/pull/1103)
* Fix linked list INSERT_BEFORE corruption and array insertdata_first ordering. [PR #1101](https://github.com/c-ares/c-ares/pull/1101)
* Fix HASH_IDX macro missing parentheses. [PR #1128](https://github.com/c-ares/c-ares/pull/1128)
* Fix malloc(0) in ares_htable_all_buckets on empty table. [PR #1131](https://github.com/c-ares/c-ares/pull/1131)
* Fix wrong sizeof in QNX confstr() call truncating domain names. [PR #1130](https://github.com/c-ares/c-ares/pull/1130)
* Clear probe pending flag after timeout. [PR #1059](https://github.com/c-ares/c-ares/pull/1059)
* Fix incorrect check for empty wide string. [PR #1064](https://github.com/c-ares/c-ares/pull/1064)
* Handle strdup failure. [PR #1077](https://github.com/c-ares/c-ares/pull/1077)
* doc: reference ares_free_string(), not ares_free(). [PR #1084](https://github.com/c-ares/c-ares/pull/1084)

Thanks go to these friendly people for their efforts and contributions for this
release:

* (@Alb3e3)
* Brad House (@bradh352)
* (@dankmeme01)
* David Hotham (@dimbleby)
* Tom Flynn (@Flynnzaa)
* (@jmestwa-coder)
* (@kodareef5)
* Kaixuan Li (@MarkLee131)
* (@lifenjoiner)
* (@metsw24-max)
* Song Li (@SongTonyLi)
* (@uwezkhan)


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


