# HTTPS

    Stability: 2 - Stable

HTTPS is the HTTP protocol over TLS/SSL. In Node.js this is implemented as a
separate module.

## Class: https.Agent
<!-- YAML
added: v0.4.4
-->

**Inherits:** [`http.Agent`]

An Agent object for HTTPS.

## Class: https.Server
<!-- YAML
added: v0.3.4
-->

**Inherits:** [`tls.Server`]
**Implements:** [`http.Server`]

`https.Server` includes the following events, methods, and properties:

### server.setTimeout(msecs, callback)
<!-- YAML
added: v0.11.2
-->

See [`http.Server#setTimeout()`].

### server.timeout
<!-- YAML
added: v0.11.2
-->

See [`http.Server#timeout`].

## https.createServer(options[, callback])
<!-- YAML
added: v0.3.4
-->

* `options` {Object} This object is used to configure the underlying [`tls.Server`].
  See [`tls.createServer()`] for valid properties.
* `callback` {Function} If supplied, `callback` is automatically added as a
  `request` event handler
* Return: [`https.Server`] instance

Creates a new HTTPS server instance.

Examples:

```js
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


// Or use a .pfx file which contains both the private key *and* certificate
const optionsPfx = {
  pfx: fs.readFileSync('server.pfx')
};

https.createServer(optionsPfx, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
}).listen(8001);
```

## https.get(options[, callback])
<!-- YAML
added: v0.3.6
-->

* `options` {Object | String} Contains request and connection details. If `options`
  is a string, it is automatically parsed with [`url.parse()`]. If `options` is
  an object, valid properties include those for [`http.request()`], except
  `protocol` defaults to `'https:'`. Additionally, options for [`tls.connect()`]
  can be specified (however, the [`https.globalAgent`] silently ignores these).
* `callback` {Function} If supplied, `callback` is automatically added as a
  `response` event handler
* Return: [`http.ClientRequest`] instance

This function is a convenience wrapper function around [`https.request()`] that
presets `method: 'GET'` in `options` and automatically calls `request.end()` on
the request object.

Example:

```js
const https = require('https');

https.get('https://encrypted.google.com/', (res) => {
  console.log('statusCode: ', res.statusCode);
  console.log('headers: ', res.headers);

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

Global instance of [`https.Agent`] for all HTTPS client requests.

## https.request(options[, callback])
<!-- YAML
added: v0.3.6
-->

* `options` {Object | String} Contains request and connection details. If `options`
  is a string, it is automatically parsed with [`url.parse()`]. If `options` is
  an object, valid properties include those for [`http.request()`], except
  `protocol` defaults to `'https:'`. Additionally, options for [`tls.connect()`]
  can be specified (however, the [`https.globalAgent`] silently ignores these).
* `callback` {Function} If supplied, `callback` is automatically added as a
  `response` event handler
* Return: [`http.ClientRequest`] instance

Creates and initiates a request to an HTTPS server.

Example:

```js
const https = require('https');

const options = {
  host: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET'
};

const req = https.request(options, (res) => {
  console.log('statusCode: ', res.statusCode);
  console.log('headers: ', res.headers);

  res.on('data', (d) => {
    process.stdout.write(d);
  });
}).on('error', (e) => {
  console.error(e);
});

req.end();
```

Example: Use a custom [`https.Agent`] to pass options to [`tls.connect()`]

```js
const https = require('https');

const options = {
  host: 'encrypted.google.com',
  port: 443,
  path: '/',
  method: 'GET',

  agent: new https.Agent({
    // `tls.connect()`-specific options
    secureProtocol: 'TLSv1_2_method'
  })
};

const req = https.request(options, (res) => {
  console.log('statusCode: ', res.statusCode);
  console.log('headers: ', res.headers);

  res.on('data', (d) => {
    process.stdout.write(d);
  });
}).on('error', (e) => {
  console.error(e);
});

req.end();
```

[`http.Agent`]: http.html#http_class_http_agent
[`http.ClientRequest`]: http.html#http_class_http_clientrequest
[`http.request()`]: http.html#http_http_request_options_callback
[`http.Server`]: http.html#http_class_http_server
[`http.Server#setTimeout()`]: http.html#http_server_settimeout_msecs_callback
[`http.Server#timeout`]: http.html#http_server_timeout
[`https.Agent`]: #https_class_https_agent
[`https.globalAgent`]: #https_https_globalagent
[`https.request()`]: #https_https_request_options_callback
[`https.Server`]: #https_class_https_server
[`tls.connect()`]: tls.html#tls_tls_connect_options_callback
[`tls.createServer()`]: tls.html#tls_tls_createserver_options_secureconnectionlistener
[`tls.Server`]: tls.html#tls_class_tls_server
[`url.parse()`]: url.html#url_url_parse_urlstring_parsequerystring_slashesdenotehost
