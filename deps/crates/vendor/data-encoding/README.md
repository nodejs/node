[![CI Status][ci_badge]][ci]
[![Coverage Status][coveralls_badge]][coveralls]

This library provides the following common encodings:

| Name                     | Description                                            |
|--------------------------|--------------------------------------------------------|
| `HEXLOWER`               | lowercase hexadecimal                                  |
| `HEXLOWER_PERMISSIVE`    | lowercase hexadecimal (case-insensitive decoding)      |
| `HEXUPPER`               | uppercase hexadecimal                                  |
| `HEXUPPER_PERMISSIVE`    | uppercase hexadecimal (case-insensitive decoding)      |
| `BASE32`                 | RFC4648 base32                                         |
| `BASE32_NOPAD`           | RFC4648 base32 (no padding)                            |
| `BASE32_NOPAD_NOCASE`    | RFC4648 base32 (no padding, case-insensitive decoding) |
| `BASE32_NOPAD_VISUAL`    | RFC4648 base32 (no padding, visual-approx. decoding)   |
| `BASE32HEX`              | RFC4648 base32hex                                      |
| `BASE32HEX_NOPAD`        | RFC4648 base32hex (no padding)                         |
| `BASE32_DNSSEC`          | RFC5155 base32                                         |
| `BASE32_DNSCURVE`        | DNSCurve base32                                        |
| `BASE64`                 | RFC4648 base64                                         |
| `BASE64_NOPAD`           | RFC4648 base64 (no padding)                            |
| `BASE64_MIME`            | RFC2045-like base64                                    |
| `BASE64_MIME_PERMISSIVE` | RFC2045-like base64 (ignoring trailing bits)           |
| `BASE64URL`              | RFC4648 base64url                                      |
| `BASE64URL_NOPAD`        | RFC4648 base64url (no padding)                         |

It also provides the possibility to define custom little-endian ASCII
base-conversion encodings for bases of size 2, 4, 8, 16, 32, and 64 (for which
all above use-cases are particular instances).

See the [documentation] for more details.

[ci]: https://github.com/ia0/data-encoding/actions/workflows/ci.yml
[ci_badge]: https://github.com/ia0/data-encoding/actions/workflows/ci.yml/badge.svg
[coveralls]: https://coveralls.io/github/ia0/data-encoding?branch=main
[coveralls_badge]: https://coveralls.io/repos/github/ia0/data-encoding/badge.svg?branch=main
[documentation]: https://docs.rs/data-encoding
