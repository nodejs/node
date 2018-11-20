# npm-registry-fetch [![npm version](https://img.shields.io/npm/v/npm-registry-fetch.svg)](https://npm.im/npm-registry-fetch) [![license](https://img.shields.io/npm/l/npm-registry-fetch.svg)](https://npm.im/npm-registry-fetch) [![Travis](https://img.shields.io/travis/npm/npm-registry-fetch/latest.svg)](https://travis-ci.org/npm/npm-registry-fetch) [![AppVeyor](https://img.shields.io/appveyor/ci/zkat/npm-registry-fetch/latest.svg)](https://ci.appveyor.com/project/npm/npm-registry-fetch) [![Coverage Status](https://coveralls.io/repos/github/npm/npm-registry-fetch/badge.svg?branch=latest)](https://coveralls.io/github/npm/npm-registry-fetch?branch=latest)

[`npm-registry-fetch`](https://github.com/npm/npm-registry-fetch) is a Node.js
library that implements a `fetch`-like API for accessing npm registry APIs
consistently. It's able to consume npm-style configuration values and has all
the necessary logic for picking registries, handling scopes, and dealing with
authentication details built-in.

This package is meant to replace the older
[`npm-registry-client`](https://npm.im/npm-registry-client).

## Example

```javascript
const npmFetch = require('npm-registry-fetch')

console.log(
  await npmFetch.json('/-/ping')
)
```

## Table of Contents

* [Installing](#install)
* [Example](#example)
* [Contributing](#contributing)
* [API](#api)
  * [`fetch`](#fetch)
  * [`fetch.json`](#fetch-json)
  * [`fetch` options](#fetch-opts)

### Install

`$ npm install npm-registry-fetch`

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

All participants and maintainers in this project are expected to follow [Code of
Conduct](CODE_OF_CONDUCT.md), and just generally be excellent to each other.

Please refer to the [Changelog](CHANGELOG.md) for project history details, too.

Happy hacking!

### API

#### <a name="fetch"></a> `> fetch(url, [opts]) -> Promise<Response>`

Performs a request to a given URL.

The URL can be either a full URL, or a path to one. The appropriate registry
will be automatically picked if only a URL path is given.

For available options, please see the section on [`fetch` options](#fetch-opts).

##### Example

```javascript
const res = await fetch('/-/ping')
console.log(res.headers)
res.on('data', d => console.log(d.toString('utf8')))
```

#### <a name="fetch-json"></a> `> fetch.json(url, [opts]) -> Promise<ResponseJSON>`

Performs a request to a given registry URL, parses the body of the response as
JSON, and returns it as its final value. This is a utility shorthand for
`fetch(url).then(res => res.json())`.

For available options, please see the section on [`fetch` options](#fetch-opts).

##### Example

```javascript
const res = await fetch.json('/-/ping')
console.log(res) // Body parsed as JSON
```

#### <a name="fetch-opts"></a> `fetch` Options

Fetch options are optional, and can be passed in as either a Map-like object
(one with a `.get()` method), a plain javascript object, or a
[`figgy-pudding`](https://npm.im/figgy-pudding) instance.

##### <a name="opts-auth-token"></a> `opts._authToken`

* Type: String
* Default: null

Authentication token string.

Can be scoped to a registry by using a "nerf dart" for that registry. That is:

```
{
  '//registry.npmjs.org/:_authToken': 't0k3nH34r'
}
```

##### <a name="opts-agent"></a> `opts.agent`

* Type: http.Agent
* Default: an appropriate agent based on URL protocol and proxy settings

An [`Agent`](https://nodejs.org/api/http.html#http_class_http_agent) instance to
be shared across requests. This allows multiple concurrent `fetch` requests to
happen on the same socket.

You do _not_ need to provide this option unless you want something particularly
specialized, since proxy configurations and http/https agents are already
automatically managed internally when this option is not passed through.

##### <a name="opts-body"></a> `opts.body`

* Type: Buffer | Stream | Object
* Default: null

Request body to send through the outgoing request. Buffers and Streams will be
passed through as-is, with a default `content-type` of
`application/octet-stream`. Plain JavaScript objects will be `JSON.stringify`ed
and the `content-type` will default to `application/json`.

Use [`opts.headers`](#opts-headers) to set the content-type to something else.

##### <a name="opts-ca"></a> `opts.ca`

* Type: String, Array, or null
* Default: null

The Certificate Authority signing certificate that is trusted for SSL
connections to the registry. Values should be in PEM format (Windows calls it
"Base-64 encoded X.509 (.CER)") with newlines replaced by the string `'\n'`. For
example:

```
{
  ca: '-----BEGIN CERTIFICATE-----\nXXXX\nXXXX\n-----END CERTIFICATE-----'
}
```

Set to `null` to only allow "known" registrars, or to a specific CA cert
to trust only that specific signing authority.

Multiple CAs can be trusted by specifying an array of certificates instead of a
single string.

See also [`opts.strict-ssl`](#opts-strict-ssl), [`opts.ca`](#opts-ca) and
[`opts.key`](#opts-key)

##### <a name="opts-cache"></a> `opts.cache`

* Type: path
* Default: null

The location of the http cache directory. If provided, certain cachable requests
will be cached according to [IETF RFC 7234](https://tools.ietf.org/html/rfc7234)
rules. This will speed up future requests, as well as make the cached data
available offline if necessary/requested.

See also [`offline`](#opts-offline), [`prefer-offline`](#opts-prefer-offline),
and [`prefer-online`](#opts-prefer-online).

##### <a name="opts-cert"></a> `opts.cert`

* Type: String
* Default: null

A client certificate to pass when accessing the registry.  Values should be in
PEM format (Windows calls it "Base-64 encoded X.509 (.CER)") with newlines
replaced by the string `'\n'`. For example:

```
{
  cert: '-----BEGIN CERTIFICATE-----\nXXXX\nXXXX\n-----END CERTIFICATE-----'
}
```

It is _not_ the path to a certificate file (and there is no "certfile" option).

See also: [`opts.ca`](#opts-ca) and [`opts.key`](#opts-key)

##### <a name="opts-fetch-retries"></a> `opts.fetch-retries`

* Type: Number
* Default: 2

The "retries" config for [`retry`](https://npm.im/retry) to use when fetching
packages from the registry.

See also [`opts.retry`](#opts-retry) to provide all retry options as a single
object.

##### <a name="opts-fetch-retry-factor"></a> `opts.fetch-retry-factor`

* Type: Number
* Default: 10

The "factor" config for [`retry`](https://npm.im/retry) to use when fetching
packages.

See also [`opts.retry`](#opts-retry) to provide all retry options as a single
object.

##### <a name="opts-fetch-retry-mintimeout"></a> `opts.fetch-retry-mintimeout`

* Type: Number
* Default: 10000 (10 seconds)

The "minTimeout" config for [`retry`](https://npm.im/retry) to use when fetching
packages.

See also [`opts.retry`](#opts-retry) to provide all retry options as a single
object.

##### <a name="opts-fetch-retry-maxtimeout"></a> `opts.fetch-retry-maxtimeout`

* Type: Number
* Default: 60000 (1 minute)

The "maxTimeout" config for [`retry`](https://npm.im/retry) to use when fetching
packages.

See also [`opts.retry`](#opts-retry) to provide all retry options as a single
object.

##### <a name="opts-headers"></a> `opts.headers`

* Type: Object
* Default: null

Additional headers for the outgoing request. This option can also be used to
override headers automatically generated by `npm-registry-fetch`, such as
`Content-Type`.

##### <a name="opts-integrity"></a> `opts.integrity`

* Type: String | [SRI object](https://npm.im/ssri)
* Default: null

If provided, the response body's will be verified against this integrity string,
using [`ssri`](https://npm.im/ssri). If verification succeeds, the response will
complete as normal. If verification fails, the response body will error with an
`EINTEGRITY` error.

Body integrity is only verified if the body is actually consumed to completion --
that is, if you use `res.json()`/`res.buffer()`, or if you consume the default
`res` stream data to its end.

Cached data will have its integrity automatically verified using the
previously-generated integrity hash for the saved request information, so
`EINTEGRITY` errors can happen if [`opts.cache`](#opts-cache) is used, even if
`opts.integrity` is not passed in.

##### <a name='opts-is-from-ci'></a> `opts.is-from-ci`

* Alias: `opts.isFromCI`
* Type: Boolean
* Default: Based on environment variables

This is used to populate the `npm-in-ci` request header sent to the registry.

##### <a name="opts-key"></a> `opts.key`

* Type: String
* Default: null

A client key to pass when accessing the registry.  Values should be in PEM
format with newlines replaced by the string `'\n'`. For example:

```
{
  key: '-----BEGIN PRIVATE KEY-----\nXXXX\nXXXX\n-----END PRIVATE KEY-----'
}
```

It is _not_ the path to a key file (and there is no "keyfile" option).

See also: [`opts.ca`](#opts-ca) and [`opts.cert`](#opts-cert)

##### <a name="opts-local-address"></a> `opts.local-address`

* Type: IP Address String
* Default: null

The IP address of the local interface to use when making connections
to the registry.

See also [`opts.proxy`](#opts-proxy)

##### <a name="opts-log"></a> `opts.log`

* Type: [`npmlog`](https://npm.im/npmlog)-like
* Default: null

Logger object to use for logging operation details. Must have the same methods
as `npmlog`.

##### <a name="opts-maxsockets"></a> `opts.maxsockets`

* Alias: `opts.max-sockets`
* Type: Integer
* Default: 12

Maximum number of sockets to keep open during requests. Has no effect if
[`opts.agent`](#opts-agent) is used.

##### <a name="opts-method"></a> `opts.method`

* Type: String
* Default: 'GET'

HTTP method to use for the outgoing request. Case-insensitive.

##### <a name="opts-noproxy"></a> `opts.noproxy`

* Type: Boolean
* Default: process.env.NOPROXY

If true, proxying will be disabled even if [`opts.proxy`](#opts-proxy) is used.

##### <a name="opts-npm-session"></a> `opts.npm-session`

* Alias: `opts.npmSession`
* Type: String
* Default: null

If provided, will be sent in the `npm-session` header. This header is used by
the npm registry to identify individual user sessions (usually individual
invocations of the CLI).

##### <a name="opts-offline"></a> `opts.offline`

* Type: Boolean
* Default: false

Force offline mode: no network requests will be done during install. To allow
`npm-registry-fetch` to fill in missing cache data, see
[`opts.prefer-offline`](#opts-prefer-offline).

This option is only really useful if you're also using
[`opts.cache`](#opts-cache).

##### <a name="opts-otp"></a> `opts.otp`

* Type: Number | String
* Default: null

This is a one-time password from a two-factor authenticator. It is required for
certain registry interactions when two-factor auth is enabled for a user
account.

##### <a name="opts-password"></a> `opts.password`

* Alias: _password
* Type: String
* Default: null

Password used for basic authentication. For the more modern authentication
method, please use the (more secure) [`opts._authToken`](#opts-auth-token)

Can optionally be scoped to a registry by using a "nerf dart" for that registry.
That is:

```
{
  '//registry.npmjs.org/:password': 't0k3nH34r'
}
```

See also [`opts.username`](#opts-username)

##### <a name="opts-prefer-offline"></a> `opts.prefer-offline`

* Type: Boolean
* Default: false

If true, staleness checks for cached data will be bypassed, but missing data
will be requested from the server. To force full offline mode, use
[`opts.offline`](#opts-offline).

This option is generally only useful if you're also using
[`opts.cache`](#opts-cache).

##### <a name="opts-prefer-online"></a> `opts.prefer-online`

* Type: Boolean
* Default: false

If true, staleness checks for cached data will be forced, making the CLI look
for updates immediately even for fresh package data.

This option is generally only useful if you're also using
[`opts.cache`](#opts-cache).


##### <a name="opts-project-scope"></a> `opts.project-scope`

* Alias: `opts.projectScope`
* Type: String
* Default: null

If provided, will be sent in the `npm-scope` header. This header is used by the
npm registry to identify the toplevel package scope that a particular project
installation is using.

##### <a name="opts-proxy"></a> `opts.proxy`

* Type: url
* Default: null

A proxy to use for outgoing http requests. If not passed in, the `HTTP(S)_PROXY`
environment variable will be used.

##### <a name="opts-query"></a> `opts.query`

* Type: String | Object
* Default: null

If provided, the request URI will have a query string appended to it using this
query. If `opts.query` is an object, it will be converted to a query string
using
[`querystring.stringify()`](https://nodejs.org/api/querystring.html#querystring_querystring_stringify_obj_sep_eq_options).

If the request URI already has a query string, it will be merged with
`opts.query`, preferring `opts.query` values.

##### <a name="opts-refer"></a> `opts.refer`

* Alias: `opts.referer`
* Type: String
* Default: null

Value to use for the `Referer` header. The npm CLI itself uses this to serialize
the npm command line using the given request.

##### <a name="opts-registry"></a> `opts.registry`

* Type: URL
* Default: `'https://registry.npmjs.org'`

Registry configuration for a request. If a request URL only includes the URL
path, this registry setting will be prepended. This configuration is also used
to determine authentication details, so even if the request URL references a
completely different host, `opts.registry` will be used to find the auth details
for that request.

See also [`opts.scope`](#opts-scope), [`opts.spec`](#opts-spec), and
[`opts.<scope>:registry`](#opts-scope-registry) which can all affect the actual
registry URL used by the outgoing request.

##### <a name="opts-retry"></a> `opts.retry`

* Type: Object
* Default: null

Single-object configuration for request retry settings. If passed in, will
override individually-passed `fetch-retry-*` settings.

##### <a name="opts-scope"></a> `opts.scope`

* Type: String
* Default: null

Associate an operation with a scope for a scoped registry. This option can force
lookup of scope-specific registries and authentication.

See also [`opts.<scope>:registry`](#opts-scope-registry) and
[`opts.spec`](#opts-spec) for interactions with this option.

##### <a name="opts-scope-registry"></a> `opts.<scope>:registry`

* Type: String
* Default: null

This option type can be used to configure the registry used for requests
involving a particular scope. For example, `opts['@myscope:registry'] =
'https://scope-specific.registry/'` will make it so requests go out to this
registry instead of [`opts.registry`](#opts-registry) when
[`opts.scope`](#opts-scope) is used, or when [`opts.spec`](#opts-spec) is a
scoped package spec.

The `@` before the scope name is optional, but recommended.

##### <a name="opts-spec"></a> `opts.spec`

* Type: String | [`npm-registry-arg`](https://npm.im/npm-registry-arg) object.
* Default: null

If provided, can be used to automatically configure [`opts.scope`](#opts-scope)
based on a specific package name. Non-registry package specs will throw an
error.

##### <a name="opts-strict-ssl"></a> `opts.strict-ssl`

* Type: Boolean
* Default: true

Whether or not to do SSL key validation when making requests to the
registry via https.

See also [`opts.ca`](#opts-ca).

##### <a name="opts-timeout"></a> `opts.timeout`

* Type: Milliseconds
* Default: 30000 (30 seconds)

Time before a hanging request times out.

##### <a name="opts-user-agent"></a> `opts.user-agent`

* Type: String
* Default: `'npm-registry-fetch@<version>/node@<node-version>+<arch> (<platform>)'`

User agent string to send in the `User-Agent` header.

##### <a name="opts-username"></a> `opts.username`

* Type: String
* Default: null

Username used for basic authentication. For the more modern authentication
method, please use the (more secure) [`opts._authToken`](#opts-auth-token)

Can optionally be scoped to a registry by using a "nerf dart" for that registry.
That is:

```
{
  '//registry.npmjs.org/:username': 't0k3nH34r'
}
```

See also [`opts.password`](#opts-password)

##### <a name="opts-auth"></a> `opts._auth`

* Type: String
* Default: null

** DEPRECATED ** This is a legacy authentication token supported only for
*compatibility. Please us [`opts._authToken`](#opts-auth-token) instead.
