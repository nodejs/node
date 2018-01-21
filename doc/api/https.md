# HTTPS

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

HTTPS is the HTTP protocol over TLS/SSL. In Node.js this is implemented as a
separate module.

## Class: https.Agent
<!-- YAML
added: v0.4.5
-->

An Agent object for HTTPS similar to [`http.Agent`][].  See [`https.request()`][]
for more information.

## Class: https.Server
<!-- YAML
added: v0.3.4
-->

This class is a subclass of `tls.Server` and emits events same as
[`http.Server`][]. See [`http.Server`][] for more information.

### server.close([callback])
<!-- YAML
added: v0.1.90
-->
- `callback` {Function}

See [`server.close()`][`http.close()`] from the HTTP module for details.

### server.listen()

Starts the HTTPS server listening for encrypted connections.
This method is identical to [`server.listen()`][] from [`net.Server`][].

### server.setTimeout([msecs][, callback])
<!-- YAML
added: v0.11.2
-->
- `msecs` {number} Defaults to 120000 (2 minutes).
- `callback` {Function}

See [`http.Server#setTimeout()`][].

### server.timeout
<!-- YAML
added: v0.11.2
-->
- {number} Defaults to 120000 (2 minutes).

See [`http.Server#timeout`][].

### server.keepAliveTimeout
<!-- YAML
added: v8.0.0
-->
- {number} Defaults to 5000 (5 seconds).

See [`http.Server#keepAliveTimeout`][].

## https.createServer([options][, requestListener])
<!-- YAML
added: v0.3.4
-->
- `options` {Object} Accepts `options` from [`tls.createServer()`][] and [`tls.createSecureContext()`][].
- `requestListener` {Function} A listener to be added to the `request` event.

Example:

```js
// curl -k https://localhost:8000/
const https = require('https');
const fs = require('fs');

const options = {
  key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
  cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem')
};

https.createServer(options, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
}).listen(8000);
```

Or

```js
const https = require('https');
const fs = require('fs');

const options = {
  pfx: fs.readFileSync('test/fixtures/test_cert.pfx'),
  passphrase: 'sample'
};

https.createServer(options, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
}).listen(8000);
```

## https.get(options[, callback])
<!-- YAML
added: v0.3.6
changes:
  - version: v7.5.0
    pr-url: https://github.com/nodejs/node/pull/10638
    description: The `options` parameter can be a WHATWG `URL` object.
-->
- `options` {Object | string | URL} Accepts the same `options` as
  [`https.request()`][], with the `method` always set to `GET`.
- `callback` {Function}

Like [`http.get()`][] but for HTTPS.

`options` can be an object, a string, or a [`URL`][] object. If `options` is a
string, it is automatically parsed with [`url.parse()`][]. If it is a [`URL`][]
object, it will be automatically converted to an ordinary `options` object.

Example:

```js
const https = require('https');

https.get('https://encrypted.google.com/', (res) => {
  console.log('statusCode:', res.statusCode);
  console.log('headers:', res.headers);

  res.on('data', (d) => {
    process.stdout.write(d);
  });

}).on('error', (e) => {
  console.error(e);
});
```

## https.globalAgent
<!-- YAML
added: v0.5.9
-->

Global instance of [`https.Agent`][] for all HTTPS client requests.

## https.request(options[, callback])
<!-- YAML
added: v0.3.6
changes:
  - version: v9.3.0
    pr-url: https://github.com/nodejs/node/pull/14903
    description: The `options` parameter can now include `clientCertEngine`.
  - version: v7.5.0
    pr-url: https://github.com/nodejs/node/pull/10638
    description: The `options` parameter can be a WHATWG `URL` object.
-->
- `options` {Object | string | URL} Accepts all `options` from [`http.request()`][],
  with some differences in default values:
  - `protocol` Defaults to `https:`
  - `port` Defaults to `443`.
  - `agent` Defaults to `https.globalAgent`.
- `callback` {Function}


Makes a request to a secure web server.

The following additional `options` from [`tls.connect()`][] are also accepted
when using a custom [`Agent`][]: `ca`, `cert`, `ciphers`, `clientCertEngine`,
`key`, `passphrase`, `pfx`, `rejectUnauthorized`, `secureProtocol`, `servername`

`options` can be an object, a string, or a [`URL`][] object. If `options` is a
string, it is automatically parsed with [`url.parse()`][]. If it is a [`URL`][]
object, it will be automatically converted to an ordinary `options` object.

Example:

```js
const https = require('https');

const options = {
  hostname: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET'
};

const req = https.request(options, (res) => {
  console.log('statusCode:', res.statusCode);
  console.log('headers:', res.headers);

  res.on('data', (d) => {
    process.stdout.write(d);
  });
});

req.on('error', (e) => {
  console.error(e);
});
req.end();
```
Example using options from [`tls.connect()`][]:

```js
const options = {
  hostname: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET',
  key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
  cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem')
};
options.agent = new https.Agent(options);

const req = https.request(options, (res) => {
  // ...
});
```

Alternatively, opt out of connection pooling by not using an `Agent`.

Example:

```js
const options = {
  hostname: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET',
  key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
  cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem'),
  agent: false
};

const req = https.request(options, (res) => {
  // ...
});
```

Example using a [`URL`][] as `options`:

```js
const options = new URL('https://abc:xyz@example.com');

const req = https.request(options, (res) => {
  // ...
});
```

[`Agent`]: #https_class_https_agent
[`URL`]: url.html#url_the_whatwg_url_api
[`http.Agent`]: http.html#http_class_http_agent
[`http.Server#keepAliveTimeout`]: http.html#http_server_keepalivetimeout
[`http.Server#setTimeout()`]: http.html#http_server_settimeout_msecs_callback
[`http.Server#timeout`]: http.html#http_server_timeout
[`http.Server`]: http.html#http_class_http_server
[`http.close()`]: http.html#http_server_close_callback
[`http.get()`]: http.html#http_http_get_options_callback
[`http.request()`]: http.html#http_http_request_options_callback
[`https.Agent`]: #https_class_https_agent
[`https.request()`]: #https_https_request_options_callback
[`net.Server`]: net.html#net_class_net_server
[`server.listen()`]: net.html#net_server_listen
[`tls.connect()`]: tls.html#tls_tls_connect_options_callback
[`tls.createSecureContext()`]: tls.html#tls_tls_createsecurecontext_options
[`tls.createServer()`]: tls.html#tls_tls_createserver_options_secureconnectionlistener
[`url.parse()`]: url.html#url_url_parse_urlstring_parsequerystring_slashesdenotehost
