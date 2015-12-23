# DNS

    Stability: 2 - Stable

Use `require('dns')` to access this module.

This module contains functions that belong to two different categories:

1) Functions that utilize resources similar to the operating system's
name/address resolving facilities to perform name resolution, and that do not
necessarily perform any network communication. This category contains only one
function: [`dns.lookup()`][]. __Developers looking to perform name resolution in
a way similar to that of other applications on the same operating system should
use [`dns.lookup()`][].__

Here is an example that does a lookup of `www.google.com`.

    const dns = require('dns');

    dns.lookup('www.google.com', function onLookup(err, addresses, family) {
      console.log('addresses:', addresses);
    });

2) Functions that connect to an actual DNS server to perform name resolution,
and that _always_ and _only_ use the network to perform DNS queries. This means
for instance, _the contents of "hosts" files (e.g. `/etc/hosts`) are not
considered_. This category contains all functions in the `dns` module but
[`dns.lookup()`][]. These functions should be used by developers who do not want
to _always_ perform queries to a nameserver over the network.

Here is an example which resolves `'www.google.com'` then reverse
resolves the IP addresses which are returned.

    const dns = require('dns');

    dns.resolve4('www.google.com', (err, addresses) => {
      if (err) throw err;

      console.log(`addresses: ${JSON.stringify(addresses)}`);

      addresses.forEach((a) => {
        dns.reverse(a, (err, hostnames) => {
          if (err) {
            throw err;
          }
          console.log(`reverse for ${a}: ${JSON.stringify(hostnames)}`);
        });
      });
    });

There are subtle consequences in choosing one or another, please consult the
[implementation considerations section][] for more information.

## dns.getServers()

Returns an array of IP addresses as strings that are currently being used for
resolution.

## dns.lookup(hostname[, options], callback)

Resolves a hostname (e.g. `'google.com'`) into the first found A (IPv4) or
AAAA (IPv6) record.

`options` can be an object or integer. If `options` is not provided, then IPv4
and IPv6 addresses are both valid. If `options` is an integer, then it must be
`4` or `6` to return addresses of that particular type.

Alternatively, `options` can be an object containing any of these properties:

* `family` {Number} - The record family. If present, must be the integer
  `4` or `6`. If not provided, both IPv4 and IPv6 addresses are accepted.
* `hints`: {Number} - If present, it should be one or more of the supported
  flags. If `hints` is not provided, then no flags are passed to the resolver.
  Multiple flags can be passed by logically OR-ing flag values.
  See [supported flags][] for more information on supported flags.
* `all`: {Boolean} - When `true`, the callback receives all resolved addresses
  in an array, instead of a single address. Defaults to `false`.

All properties are optional. An example usage of `options`:

```
{
  family: 4,
  hints: dns.ADDRCONFIG | dns.V4MAPPED,
  all: false
}
```

The `callback` has arguments `(err, address, family)`. `address` is a string
representation of an IPv4 or IPv6 address. `family` is either the integer `4` or
`6` and denotes the family of `address`.

With the `all` option set, the `callback` arguments change to
`(err, addresses)`, with `addresses` being an array of objects with `address`
and `family` properties.

When an error occurs, `err` is set to an [`Error`][] object, where `err.code` is
one of the [error codes][]. Keep in mind that `err.code` will be set to
`'ENOENT'` not only when the `hostname` does not exist but also when the lookup
fails in other ways such as when no file descriptors are available.

`dns.lookup()` doesn't necessarily have anything to do with the DNS protocol.
It's only an operating system facility that can associate name with addresses,
and vice versa. Its implementation can have subtle but important consequences on
the behavior of any Node.js program. Please take some time to consult the
[Implementation considerations section][] before using it.

## dns.lookupService(address, port, callback)

Resolves the given `address` and `port` into a `hostname` and `service`.

The callback has arguments `(err, hostname, service)`. The `hostname` and
`service` arguments are strings (e.g. `'localhost'` and `'http'` respectively).

When an error occurs, `err` is set to an [`Error`][] object, where `err.code` is
one of the [error codes][].


## dns.resolve(hostname[, rrtype], callback)

Resolves a `hostname` (e.g. `'google.com'`) into an array of the record types
specified by `rrtype`.

Valid `rrtype`s are:

 * `'A'` (IPv4 address records) **(default)**
 * `'AAAA'` (IPv6 address records)
 * `'CNAME'` (canonical name records)
 * `'LOC'` (geographical location records)
 * `'MX'` (mail exchange records)
 * `'NAPTR'` (name authority pointer records)
 * `'NS'` (name server records)
 * `'PTR'` (reverse IP lookup)
 * `'SOA'` (start of authority record)
 * `'SRV'` (service records)
 * `'SSHP'` (SSH host fingerprint records)
 * `'TXT'` (text records)

The `callback` has arguments `(err, addresses)`.  The type of each item in
`addresses` is determined by the record type and is described in the
documentation for the corresponding `dns.resolve*()` methods.

When an error occurs, `err` is set to an [`Error`][] object, where `err.code` is
one of the [error codes][].


## dns.resolve4(hostname, callback)

The same as [`dns.resolve()`][], but only for IPv4 (`A`) records.

`addresses` is an array of IPv4 addresses
(e.g. `['74.125.79.104', '74.125.79.105', '74.125.79.106']`).

## dns.resolve6(hostname, callback)

The same as [`dns.resolve4()`][] except for IPv6 (`AAAA`) records.

## dns.resolveCname(hostname, callback)

The same as [`dns.resolve()`][], but only for canonical name (`CNAME`) records.

`addresses` is an array of the `CNAME` records available for `hostname`
(e.g., `['bar.example.com']`).

## dns.resolveLoc(hostname, callback)

The same as [`dns.resolve()`][], but only for geographical location (`LOC`)
records.

`addresses` is an array of the `LOC` records available for `hostname`.

`LOC` records have the following structure:

```
{
  size: 0,
  horizPrec: 22,
  vertPrec: 19,
  latitude: 2336026648,
  longitude: 2165095648,
  altitude: 9999800
}
```

## dns.resolveMx(hostname, callback)

The same as [`dns.resolve()`][], but only for mail exchange (`MX`) records.

`addresses` is an array of `MX` records available for `hostname`.

`MX` records have the following structure:

```
{
  priority: 10,
  exchange: 'mx.example.com'
}
```

## dns.resolveNaptr(hostname, callback)

The same as [`dns.resolve()`][], but only for name authority pointer (`NAPTR`)
records.

`addresses` is an array of the `NAPTR` records available for `hostname`.

`NAPTR` records have the following structure:

```
{
  flags: 'u',
  service: 'foo',
  regexp: '!^.*$!https://www.example.net!',
  replacement: '',
  order: 100,
  preference: 100
}
```

## dns.resolveNs(hostname, callback)

The same as [`dns.resolve()`][], but only for name server (`NS`) records.

`addresses` is an array of the `NS` records available for `hostname`
(e.g., `['ns1.example.com', 'ns2.example.com']`).

## dns.resolveSoa(hostname, callback)

The same as [`dns.resolve()`][], but only for start of authority (`SOA`)
records.

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

## dns.resolveSrv(hostname, callback)

The same as [`dns.resolve()`][], but only for service (`SRV`) records.

`addresses` is an array of the `SRV` records available for `hostname`.

`SRV` records have the following structure:

```
{
  priority: 10,
  weight: 5,
  port: 21223,
  name: 'service.example.com'
}
```

## dns.resolveSshp(hostname, callback)

The same as [`dns.resolve()`][], but only for SSH host fingerprint (`SSHP`)
records.

`addresses` is an array of the `SSHP` records available for `hostname`.

`SSHFP` records have the following structure:

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

## dns.resolveTxt(hostname, callback)

The same as [`dns.resolve()`][], but only for text (`TXT`) records.

`addresses` is a 2-D array of the `TXT` records available for `hostname`. Each
sub-array contains one or more chunks of one `TXT` record. Depending on the use
case, they could be either joined together or treated separately.

`TXT` records may look like:

```
[ 'v=spf1 ip4:0.0.0.0 ', '~all' ]
```

## dns.reverse(ip, callback)

Reverse resolves an ip address to an array of hostnames.

The callback has arguments `(err, hostnames)`.

On error, `err` is an [`Error`][] object, where `err.code` is
one of the error codes listed below.

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

## Supported flags

The following flags can be passed as `hints` to [`dns.lookup()`][].

- `dns.ADDRCONFIG`: Returned address types are determined by the types
of addresses supported by the current system. For example, IPv4 addresses
are only returned if the current system has at least one IPv4 address
configured. Loopback addresses are not considered.
- `dns.V4MAPPED`: If the IPv6 family was specified, but no IPv6 addresses were
found, then return IPv4-mapped IPv6 addresses.

## Implementation considerations

Although [`dns.lookup()`][] and `dns.resolve*()/dns.reverse()` functions have
the same goal of associating a network name with a network address (or vice
versa), their behavior is quite different. These differences can have subtle but
significant consequences on the behavior of Node.js programs.

### dns.lookup

Under the hood, [`dns.lookup()`][] utilizes resources similar to the operating
system's facilities used by many other programs. For instance,
[`dns.lookup()`][] will almost always resolve a given name the same way as the
`ping` command. On most POSIX-like operating systems, the behavior of the
[`dns.lookup()`][] function can be tweaked by changing settings in
`nsswitch.conf(5)` and/or `resolv.conf(5)`, but be careful that changing these
files will change the behavior of all other programs running on the same
operating system.

### dns.resolve, functions starting with dns.resolve and dns.reverse

These functions are implemented a little differently than [`dns.lookup()`][], in
that they _always_ and _only_ perform DNS queries over the network. This means
for instance, _the contents of "hosts" files (e.g. `/etc/hosts`) are not
considered_.

[`dns.lookup()`]: #dns_dns_lookup_hostname_options_callback
[`dns.resolve()`]: #dns_dns_resolve_hostname_rrtype_callback
[`dns.resolve4()`]: #dns_dns_resolve4_hostname_callback
[`Error`]: errors.html#errors_class_error
[implementation considerations section]: #dns_implementation_considerations
[supported flags]: #dns_supported_flags
[error codes]: #dns_error_codes
