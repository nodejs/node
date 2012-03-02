# DNS

    Stability: 3 - Stable

Use `require('dns')` to access this module. All methods in the dns module
use C-Ares except for `dns.lookup` which uses `getaddrinfo(3)` in a thread
pool. C-Ares is much faster than `getaddrinfo` but the system resolver is
more constant with how other programs operate. When a user does
`net.connect(80, 'google.com')` or `http.get({ host: 'google.com' })` the
`dns.lookup` method is used. Users who need to do a large number of look ups
quickly should use the methods that go through C-Ares.

Here is an example which resolves `'www.google.com'` then reverse
resolves the IP addresses which are returned.

    var dns = require('dns');

    dns.resolve4('www.google.com', function (err, addresses) {
      if (err) throw err;

      console.log('addresses: ' + JSON.stringify(addresses));

      addresses.forEach(function (a) {
        dns.reverse(a, function (err, domains) {
          if (err) {
            console.log('reverse for ' + a + ' failed: ' +
              err.message);
          } else {
            console.log('reverse for ' + a + ': ' +
              JSON.stringify(domains));
          }
        });
      });
    });

## dns.lookup(domain, [family], callback)

Resolves a domain (e.g. `'google.com'`) into the first found A (IPv4) or
AAAA (IPv6) record.
The `family` can be the integer `4` or `6`. Defaults to `null` that indicates
both Ip v4 and v6 address family.

The callback has arguments `(err, address, family)`.  The `address` argument
is a string representation of a IP v4 or v6 address. The `family` argument
is either the integer 4 or 6 and denotes the family of `address` (not
necessarily the value initially passed to `lookup`).


## dns.resolve(domain, [rrtype], callback)

Resolves a domain (e.g. `'google.com'`) into an array of the record types
specified by rrtype. Valid rrtypes are `'A'` (IPV4 addresses, default),
`'AAAA'` (IPV6 addresses), `'MX'` (mail exchange records), `'TXT'` (text
records), `'SRV'` (SRV records), `'PTR'` (used for reverse IP lookups),
`'NS'` (name server records) and `'CNAME'` (canonical name records).

The callback has arguments `(err, addresses)`.  The type of each item
in `addresses` is determined by the record type, and described in the
documentation for the corresponding lookup methods below.

On error, `err` would be an instanceof `Error` object, where `err.errno` is
one of the error codes listed below and `err.message` is a string describing
the error in English.


## dns.resolve4(domain, callback)

The same as `dns.resolve()`, but only for IPv4 queries (`A` records).
`addresses` is an array of IPv4 addresses (e.g.
`['74.125.79.104', '74.125.79.105', '74.125.79.106']`).

## dns.resolve6(domain, callback)

The same as `dns.resolve4()` except for IPv6 queries (an `AAAA` query).


## dns.resolveMx(domain, callback)

The same as `dns.resolve()`, but only for mail exchange queries (`MX` records).

`addresses` is an array of MX records, each with a priority and an exchange
attribute (e.g. `[{'priority': 10, 'exchange': 'mx.example.com'},...]`).

## dns.resolveTxt(domain, callback)

The same as `dns.resolve()`, but only for text queries (`TXT` records).
`addresses` is an array of the text records available for `domain` (e.g.,
`['v=spf1 ip4:0.0.0.0 ~all']`).

## dns.resolveSrv(domain, callback)

The same as `dns.resolve()`, but only for service records (`SRV` records).
`addresses` is an array of the SRV records available for `domain`. Properties
of SRV records are priority, weight, port, and name (e.g.,
`[{'priority': 10, {'weight': 5, 'port': 21223, 'name': 'service.example.com'}, ...]`).

## dns.reverse(ip, callback)

Reverse resolves an ip address to an array of domain names.

The callback has arguments `(err, domains)`.

## dns.resolveNs(domain, callback)

The same as `dns.resolve()`, but only for name server records (`NS` records).
`addresses` is an array of the name server records available for `domain`
(e.g., `['ns1.example.com', 'ns2.example.com']`).

## dns.resolveCname(domain, callback)

The same as `dns.resolve()`, but only for canonical name records (`CNAME`
records). `addresses` is an array of the canonical name records available for
`domain` (e.g., `['bar.example.com']`).

If there an an error, `err` will be non-null and an instanceof the Error
object.

Each DNS query can return an error code.

- `dns.TEMPFAIL`: timeout, SERVFAIL or similar.
- `dns.PROTOCOL`: got garbled reply.
- `dns.NXDOMAIN`: domain does not exists.
- `dns.NODATA`: domain exists but no data of reqd type.
- `dns.NOMEM`: out of memory while processing.
- `dns.BADQUERY`: the query is malformed.
