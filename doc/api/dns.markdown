# DNS

    Stability: 2 - Stable

Use `require('dns')` to access this module.

This module contains functions that belong to two different categories:

1) Functions that use the underlying operating system facilities to perform
name resolution, and that do not necessarily do any network communication.
This category contains only one function: `dns.lookup`. __Developers looking
to perform name resolution in the same way that other applications on the same
operating system behave should use `dns.lookup`.__

Here is an example that does a lookup of `www.google.com`.

    var dns = require('dns');

    dns.lookup('www.google.com', function onLookup(err, addresses, family) {
      console.log('addresses:', addresses);
    });

2) Functions that connect to an actual DNS server to perform name resolution,
and that _always_ use the network to perform DNS queries. This category
contains all functions in the `dns` module but `dns.lookup`. These functions
do not use the same set of configuration files than what `dns.lookup` uses.
For instance, _they do not use the configuration from `/etc/hosts`_. These
functions should be used by developers who do not want to use the underlying
operating system's facilities for name resolution, and instead want to
_always_ perform DNS queries.

Here is an example which resolves `'www.google.com'` then reverse
resolves the IP addresses which are returned.

    var dns = require('dns');

    dns.resolve4('www.google.com', function (err, addresses) {
      if (err) throw err;

      console.log('addresses: ' + JSON.stringify(addresses));

      addresses.forEach(function (a) {
        dns.reverse(a, function (err, hostnames) {
          if (err) {
            throw err;
          }

          console.log('reverse for ' + a + ': ' + JSON.stringify(hostnames));
        });
      });
    });

There are subtle consequences in choosing one or another, please consult the
[Implementation considerations section](#dns_implementation_considerations)
for more information.

## dns.lookup(hostname[, options], callback)

Resolves a hostname (e.g. `'google.com'`) into the first found A (IPv4) or
AAAA (IPv6) record. `options` can be an object or integer. If `options` is
not provided, then IP v4 and v6 addresses are both valid. If `options` is
an integer, then it must be `4` or `6`.

Alternatively, `options` can be an object containing these properties:

* `family` {Number} - The record family. If present, must be the integer
  `4` or `6`. If not provided, both IP v4 and v6 addresses are accepted.
* `hints`: {Number} - If present, it should be one or more of the supported
  `getaddrinfo` flags. If `hints` is not provided, then no flags are passed to
  `getaddrinfo`. Multiple flags can be passed through `hints` by logically
  `OR`ing their values.
  See [supported `getaddrinfo` flags](#dns_supported_getaddrinfo_flags) below
  for more information on supported flags.
* `all`: {Boolean} - When `true`, the callback returns all resolved addresses
  in an array, otherwise returns a single address. Defaults to `false`.

All properties are optional. An example usage of options is shown below.

```
{
  family: 4,
  hints: dns.ADDRCONFIG | dns.V4MAPPED,
  all: false
}
```

The callback has arguments `(err, address, family)`. `address` is a string
representation of an IP v4 or v6 address. `family` is either the integer 4 or 6
and denotes the family of `address` (not necessarily the value initially passed
to `lookup`).

With the `all` option set, the arguments change to `(err, addresses)`, with
`addresses` being an array of objects with the properties `address` and
`family`.

On error, `err` is an `Error` object, where `err.code` is the error code.
Keep in mind that `err.code` will be set to `'ENOENT'` not only when
the hostname does not exist but also when the lookup fails in other ways
such as no available file descriptors.

`dns.lookup` doesn't necessarily have anything to do with the DNS protocol.
It's only an operating system facility that can associate name with addresses,
and vice versa.

Its implementation can have subtle but important consequences on the behavior
of any Node.js program. Please take some time to consult the [Implementation
considerations section](#dns_implementation_considerations) before using it.

## dns.lookupService(address, port, callback)

Resolves the given address and port into a hostname and service using
`getnameinfo`.

The callback has arguments `(err, hostname, service)`. The `hostname` and
`service` arguments are strings (e.g. `'localhost'` and `'http'` respectively).

On error, `err` is an `Error` object, where `err.code` is the error code.


## dns.resolve(hostname[, rrtype], callback)

Resolves a hostname (e.g. `'google.com'`) into an array of the record types
specified by rrtype.

Valid rrtypes are:

 * `'A'` (IPV4 addresses, default)
 * `'AAAA'` (IPV6 addresses)
 * `'MX'` (mail exchange records)
 * `'TXT'` (text records)
 * `'SRV'` (SRV records)
 * `'PTR'` (used for reverse IP lookups)
 * `'NS'` (name server records)
 * `'CNAME'` (canonical name records)
 * `'SOA'` (start of authority record)

The callback has arguments `(err, addresses)`.  The type of each item
in `addresses` is determined by the record type, and described in the
documentation for the corresponding lookup methods below.

On error, `err` is an `Error` object, where `err.code` is
one of the error codes listed below.


## dns.resolve4(hostname, callback)

The same as `dns.resolve()`, but only for IPv4 queries (`A` records).
`addresses` is an array of IPv4 addresses (e.g.
`['74.125.79.104', '74.125.79.105', '74.125.79.106']`).

## dns.resolve6(hostname, callback)

The same as `dns.resolve4()` except for IPv6 queries (an `AAAA` query).


## dns.resolveMx(hostname, callback)

The same as `dns.resolve()`, but only for mail exchange queries (`MX` records).

`addresses` is an array of MX records, each with a priority and an exchange
attribute (e.g. `[{'priority': 10, 'exchange': 'mx.example.com'},...]`).

## dns.resolveTxt(hostname, callback)

The same as `dns.resolve()`, but only for text queries (`TXT` records).
`addresses` is a 2-d array of the text records available for `hostname` (e.g.,
`[ ['v=spf1 ip4:0.0.0.0 ', '~all' ] ]`). Each sub-array contains TXT chunks of
one record. Depending on the use case, the could be either joined together or
treated separately.

## dns.resolveSrv(hostname, callback)

The same as `dns.resolve()`, but only for service records (`SRV` records).
`addresses` is an array of the SRV records available for `hostname`. Properties
of SRV records are priority, weight, port, and name (e.g.,
`[{'priority': 10, 'weight': 5, 'port': 21223, 'name': 'service.example.com'}, ...]`).

## dns.resolveSoa(hostname, callback)

The same as `dns.resolve()`, but only for start of authority record queries
(`SOA` record).

`addresses` is an object with the following structure:

```
{
  nsname: 'ns.example.com',
  hostmaster: 'root.example.com',
  serial: 2013101809,
  refresh: 10000,
  retry: 2400,
  expire: 604800,
  minttl: 3600
}
```

## dns.resolveNs(hostname, callback)

The same as `dns.resolve()`, but only for name server records (`NS` records).
`addresses` is an array of the name server records available for `hostname`
(e.g., `['ns1.example.com', 'ns2.example.com']`).

## dns.resolveCname(hostname, callback)

The same as `dns.resolve()`, but only for canonical name records (`CNAME`
records). `addresses` is an array of the canonical name records available for
`hostname` (e.g., `['bar.example.com']`).

## dns.reverse(ip, callback)

Reverse resolves an ip address to an array of hostnames.

The callback has arguments `(err, hostnames)`.

On error, `err` is an `Error` object, where `err.code` is
one of the error codes listed below.

## dns.getServers()

Returns an array of IP addresses as strings that are currently being used for
resolution

## dns.setServers(servers)

Given an array of IP addresses as strings, set them as the servers to use for
resolving

If you specify a port with the address it will be stripped, as the underlying
library doesn't support that.

This will throw if you pass invalid input.

## Error codes

Each DNS query can return one of the following error codes:

- `dns.NODATA`: DNS server returned answer with no data.
- `dns.FORMERR`: DNS server claims query was misformatted.
- `dns.SERVFAIL`: DNS server returned general failure.
- `dns.NOTFOUND`: Domain name not found.
- `dns.NOTIMP`: DNS server does not implement requested operation.
- `dns.REFUSED`: DNS server refused query.
- `dns.BADQUERY`: Misformatted DNS query.
- `dns.BADNAME`: Misformatted hostname.
- `dns.BADFAMILY`: Unsupported address family.
- `dns.BADRESP`: Misformatted DNS reply.
- `dns.CONNREFUSED`: Could not contact DNS servers.
- `dns.TIMEOUT`: Timeout while contacting DNS servers.
- `dns.EOF`: End of file.
- `dns.FILE`: Error reading file.
- `dns.NOMEM`: Out of memory.
- `dns.DESTRUCTION`: Channel is being destroyed.
- `dns.BADSTR`: Misformatted string.
- `dns.BADFLAGS`: Illegal flags specified.
- `dns.NONAME`: Given hostname is not numeric.
- `dns.BADHINTS`: Illegal hints flags specified.
- `dns.NOTINITIALIZED`: c-ares library initialization not yet performed.
- `dns.LOADIPHLPAPI`: Error loading iphlpapi.dll.
- `dns.ADDRGETNETWORKPARAMS`: Could not find GetNetworkParams function.
- `dns.CANCELLED`: DNS query cancelled.

## Supported getaddrinfo flags

The following flags can be passed as hints to `dns.lookup`.

- `dns.ADDRCONFIG`: Returned address types are determined by the types
of addresses supported by the current system. For example, IPv4 addresses
are only returned if the current system has at least one IPv4 address
configured. Loopback addresses are not considered.
- `dns.V4MAPPED`: If the IPv6 family was specified, but no IPv6 addresses were
found, then return IPv4 mapped IPv6 addresses. Note that it is not supported
on some operating systems (e.g FreeBSD 10.1).

## Implementation considerations

Although `dns.lookup` and `dns.resolve*/dns.reverse` functions have the same
goal of associating a network name with a network address (or vice versa),
their behavior is quite different. These differences can have subtle but
significant consequences on the behavior of Node.js programs.

### dns.lookup

Under the hood, `dns.lookup` uses the same operating system facilities as most
other programs. For instance, `dns.lookup` will almost always resolve a given
name the same way as the `ping` command. On most POSIX-like operating systems,
the behavior of the `dns.lookup` function can be tweaked by changing settings
in `nsswitch.conf(5)` and/or `resolv.conf(5)`, but be careful that changing
these files will change the behavior of all other programs running on the same
operating system.

Though the call will be asynchronous from JavaScript's perspective, it is
implemented as a synchronous call to `getaddrinfo(3)` that runs on libuv's
threadpool. Because libuv's threadpool has a fixed size, it means that if for
whatever reason the call to `getaddrinfo(3)` takes a long time, other
operations that could run on libuv's threadpool (such as filesystem
operations) will experience degraded performance. In order to mitigate this
issue, one potential solution is to increase the size of libuv's threadpool by
setting the 'UV_THREADPOOL_SIZE' environment variable to a value greater than
4 (its current default value). For more information on libuv's threadpool, see
[the official libuv
documentation](http://docs.libuv.org/en/latest/threadpool.html).

### dns.resolve, functions starting with dns.resolve and dns.reverse

These functions are implemented quite differently than `dns.lookup`. They do
not use `getaddrinfo(3)` and they _always_ perform a DNS query on the network.
This network communication is always done asynchronously, and does not use
libuv's threadpool.

As a result, these functions cannot have the same negative impact on other
processing that happens on libuv's threadpool that `dns.lookup` can have.

They do not use the same set of configuration files than what `dns.lookup`
uses. For instance, _they do not use the configuration from `/etc/hosts`_.
