urlparse
========

urlparse is a URL parser compatible to `url_parser_parse_url()` from
`nodejs/http-parser <https://github.com/nodejs/http-parser>`_ which
has been archived since 2022.  urlparse only implements the strict
mode.

There is a slight difference in a return code when they fail.
`url_parser_parse_url()` returns nonzero if it fails.
`urlparse_parse_url()` returns the negative error code
``URLPARSE_ERR_PARSE`` if it fails.

A caller needs to call `http_parser_url_init()` before
`http_parser_parse_url()`.  urlparse does not need a similar function
because `urlparse_parser_url()` initializes ``urlparse_url`` before
its use.

`url_parser_parse_url()` historically does not follow any standards
like RFC 3986.  Here is the allowed characters in each URL component:

- scheme: ``A-Za-z``
- userinfo: ``A-Za-z!$%&'()*+,-.:;=_~``
- host: ``a-zA-Z0-9-.``
- IPv6 host: ``A-Fa-f0-9.:``

  - optionally followed by zone info which starts ``%`` and can
    contain: ``A-Za-z0-9%-._~``

  - and IPv6 host must be enclosed by ``[`` and ``]``

- port: ``0-9``
- path: ``A-Za-z0-9!"$%&'()*+,-./:;<=>@[\]^_`{|}~``
- query: ``A-Za-z0-9!"$%&'()*+,-./:;<=>?@[\]^_`{|}~``
- fragment: ``A-Za-z0-9!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~``

  - all consecutive ``#`` characters that precede a fragment are
    treated as a single ``#``.  A fragment cannot start with ``#``.
