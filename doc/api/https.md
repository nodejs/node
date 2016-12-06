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

### server.setTimeout(msecs, callback)
<!-- YAML
added: v0.11.2
-->

See [`http.Server#setTimeout()`][].

### server.timeout
<!-- YAML
added: v0.11.2
-->

See [`http.Server#timeout`][].

## https.createServer(options[, requestListener])
<!-- YAML
added: v0.3.4
-->

Returns a new HTTPS web server object. The `options` is similar to
[`tls.createServer()`][].  The `requestListener` is a function which is
automatically added to the `'request'` event.

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

See [`http.close()`][] for details.

### server.listen(handle[, callback])
### server.listen(path[, callback])
### server.listen(port[, host][, backlog][, callback])

See [`http.listen()`][] for details.

## https.get(options, callback)
<!-- YAML
added: v0.3.6
-->

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

## https.request(options, callback)
<!-- YAML
added: v0.3.6
-->

Makes a request to a secure web server.

`options` can be an object or a string. If `options` is a string, it is
automatically parsed with [`url.parse()`][].

All options from [`http.request()`][] are valid.

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

The options argument has the following options

- `host`: A domain name or IP address of the server to issue the request to.
  Defaults to `'localhost'`.
- `hostname`: Alias for `host`. To support `url.parse()` `hostname` is
  preferred over `host`.
- `family`: IP address family to use when resolving `host` and `hostname`.
  Valid values are `4` or `6`. When unspecified, both IP v4 and v6 will be
  used.
- `port`: Port of remote server. Defaults to 443.
- `localAddress`: Local interface to bind for network connections.
- `socketPath`: Unix Domain Socket (use one of host:port or socketPath).
- `method`: A string specifying the HTTP request method. Defaults to `'GET'`.
- `path`: Request path. Defaults to `'/'`. Should include query string if any.
  E.G. `'/index.html?page=12'`. An exception is thrown when the request path
  contains illegal characters. Currently, only spaces are rejected but that
  may change in the future.
- `headers`: An object containing request headers.
- `auth`: Basic authentication i.e. `'user:password'` to compute an
  Authorization header.
- `agent`: Controls [`Agent`][] behavior. When an Agent is used request will
  default to `Connection: keep-alive`. Possible values:
 - `undefined` (default): use [`globalAgent`][] for this host and port.
 - `Agent` object: explicitly use the passed in `Agent`.
 - `false`: opts out of connection pooling with an Agent, defaults request to
   `Connection: close`.

The following options from [`tls.connect()`][] can also be specified:

- `pfx`: Certificate, Private key and CA certificates to use for SSL. Default `null`.
- `key`: Private key to use for SSL. Default `null`.
- `passphrase`: A string of passphrase for the private key or pfx. Default `null`.
- `cert`: Public x509 certificate to use. Default `null`.
- `ca`: A string, [`Buffer`][] or array of strings or [`Buffer`][]s of trusted
  certificates in PEM format. If this is omitted several well known "root"
  CAs will be used, like VeriSign. These are used to authorize connections.
- `ciphers`: A string describing the ciphers to use or exclude. Consult
  <https://www.openssl.org/docs/man1.0.2/apps/ciphers.html#CIPHER-LIST-FORMAT> for
  details on the format.
- `rejectUnauthorized`: If `true`, the server certificate is verified against
  the list of supplied CAs. An `'error'` event is emitted if verification
  fails. Verification happens at the connection level, *before* the HTTP
  request is sent. Default `true`.
- `secureProtocol`: The SSL method to use, e.g. `SSLv3_method` to force
  SSL version 3. The possible values depend on your installation of
  OpenSSL and are defined in the constant [`SSL_METHODS`][].
- `servername`: Servername for SNI (Server Name Indication) TLS extension.

In order to specify these options, use a custom [`Agent`][].

Example:

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
[`url.parse()`]: url.html#url_url_parse_urlstring_parsequerystring_slashesdenotehost
