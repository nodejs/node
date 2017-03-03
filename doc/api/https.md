# HTTPS

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

### server.setTimeout([msecs][, callback])
<!-- YAML
added: v0.11.2
-->
- `msecs` {number} Defaults to 120000 (2 minutes).
- `callback` {Function}

See [`http.Server#setTimeout()`][].

### server.timeout([msecs])
<!-- YAML
added: v0.11.2
-->
- `msecs` {number} Defaults to 120000 (2 minutes).

See [`http.Server#timeout`][].

## https.createServer(options[, requestListener])
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
  pfx: fs.readFileSync('server.pfx')
};

https.createServer(options, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
}).listen(8000);
```

### server.close([callback])
<!-- YAML
added: v0.1.90
-->
- `callback` {Function}

See [`http.close()`][] for details.

### server.listen(handle[, callback])
- `handle` {Object}
- `callback` {Function}

### server.listen(path[, callback])
- `path` {string}
- `callback` {Function}

### server.listen([port][, host][, backlog][, callback])
- `port` {number}
- `hostname` {string}
- `backlog` {number}
- `callback` {Function}

See [`http.listen()`][] for details.

## https.get(options[, callback])
<!-- YAML
added: v0.3.6
-->
- `options` {Object | string} Accepts the same `options` as
  [`https.request()`][], with the `method` always set to `GET`.
- `callback` {Function}

Like [`http.get()`][] but for HTTPS.

`options` can be an object or a string. If `options` is a string, it is
automatically parsed with [`url.parse()`][].

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
-->
- `options` {Object | string} Accepts all `options` from [`http.request()`][],
  with some differences in default values:
  - `protocol` Defaults to `https:`
  - `port` Defaults to `443`.
  - `agent` Defaults to `https.globalAgent`.
- `callback` {Function}


Makes a request to a secure web server.

The following additional `options` from [`tls.connect()`][] are also accepted when using a
  custom [`Agent`][]:
  `pfx`, `key`, `passphrase`, `cert`, `ca`, `ciphers`, `rejectUnauthorized`, `secureProtocol`, `servername`

`options` can be an object or a string. If `options` is a string, it is
automatically parsed with [`url.parse()`][].

Example:

```js
const https = require('https');

var options = {
  hostname: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET'
};

var req = https.request(options, (res) => {
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
var options = {
  hostname: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET',
  key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
  cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem')
};
options.agent = new https.Agent(options);

var req = https.request(options, (res) => {
  ...
});
```

Alternatively, opt out of connection pooling by not using an `Agent`.

Example:

```js
var options = {
  hostname: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET',
  key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
  cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem'),
  agent: false
};

var req = https.request(options, (res) => {
  ...
});
```

[`Agent`]: #https_class_https_agent
[`Buffer`]: buffer.html#buffer_buffer
[`globalAgent`]: #https_https_globalagent
[`http.Agent`]: http.html#http_class_http_agent
[`http.close()`]: http.html#http_server_close_callback
[`http.get()`]: http.html#http_http_get_options_callback
[`http.listen()`]: http.html#http_server_listen_port_hostname_backlog_callback
[`http.request()`]: http.html#http_http_request_options_callback
[`http.Server#setTimeout()`]: http.html#http_server_settimeout_msecs_callback
[`http.Server#timeout`]: http.html#http_server_timeout
[`http.Server`]: http.html#http_class_http_server
[`https.Agent`]: #https_class_https_agent
[`https.request()`]: #https_https_request_options_callback
[`SSL_METHODS`]: https://www.openssl.org/docs/man1.0.2/ssl/ssl.html#DEALING-WITH-PROTOCOL-METHODS
[`tls.connect()`]: tls.html#tls_tls_connect_options_callback
[`tls.createServer()`]: tls.html#tls_tls_createserver_options_secureconnectionlistener
[`tls.createSecureContext()`]: tls.html#tls_tls_createsecurecontext_options
[`url.parse()`]: url.html#url_url_parse_urlstring_parsequerystring_slashesdenotehost
