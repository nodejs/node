
2.1.0 / 2017-05-24
==================

  * DRY post-lookup logic
  * Fix an error in readme (#13, @599316527)
  * travis: test node v5
  * travis: test iojs v1, 2, 3 and node.js v4
  * test: use ssl-cert-snakeoil cert files
  * Authentication support (#9, @baryshev)

2.0.0 / 2015-07-10
==================

  * API CHANGE! Removed `secure` boolean second argument in constructor
  * upgrade to "agent-base" v2 API
  * package: update "extend" to v3

1.0.2 / 2015-07-01
==================

  * remove "v4a" from description
  * socks-proxy-agent: cast `port` to a Number
  * travis: attempt to make node v0.8 work
  * travis: test node v0.12, don't test v0.11
  * test: pass `rejectUnauthorized` as a proxy opt
  * test: catch http.ClientRequest errors
  * test: add self-signed SSL server cert files
  * test: refactor to use local SOCKS, HTTP and HTTPS servers
  * README: use SVG for Travis-CI badge

1.0.1 / 2015-03-01
==================

  * switched from using "socks-client" to "socks" (#5, @JoshGlazebrook)

1.0.0 / 2015-02-11
==================

  * add client-side DNS lookup logic for 4 and 5 version socks proxies
  * remove dead `onproxyconnect()` code function
  * use a switch statement to decide the socks `version`
  * refactor to use "socks-client" instead of "rainbowsocks"
  * package: remove "rainbowsocks" dependency
  * package: allow any "mocha" v2

0.1.2 / 2014-06-11
==================

  * package: update "rainbowsocks" to v0.1.2
  * travis: don't test node v0.9

0.1.1 / 2014-04-09
==================

  * package: update outdated dependencies
  * socks-proxy-agent: pass `secure` flag when no `new`
  * socks-proxy-agent: small code cleanup

0.1.0 / 2013-11-19
==================

  * add .travis.yml file
  * socks-proxy-agent: properly mix in the proxy options
  * socks-proxy-agent: coerce the `secureEndpoint` into a Boolean
  * socks-proxy-agent: use "extend" module
  * socks-proxy-agent: update to "agent-base" v1 API

0.0.2 / 2013-07-24
==================

  * socks-proxy-agent: properly set the `defaultPort` property

0.0.1 / 2013-07-11
==================

  * Initial release
