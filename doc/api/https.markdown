# HTTPS

    Stability: 3 - Stable

HTTPS is the HTTP protocol over TLS/SSL. In Node this is implemented as a
separate module.

## Class: https.Server

This class is a subclass of `tls.Server` and emits events same as
`http.Server`. See `http.Server` for more information.

## https.createServer(options, [requestListener])

Returns a new HTTPS web server object. The `options` is similar to
`tls.createServer()`. The `requestListener` is a function which is
automatically added to the `'request'` event.

Example:

    // curl -k https://localhost:8000/
    var https = require('https');
    var fs = require('fs');

    var options = {
      key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
      cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem')
    };

    https.createServer(options, function (req, res) {
      res.writeHead(200);
      res.end("hello world\n");
    }).listen(8000);


## https.request(options, callback)

Makes a request to a secure web server.
All options from [http.request()](http.html#http.request) are valid.

Example:

    var https = require('https');

    var options = {
      host: 'encrypted.google.com',
      port: 443,
      path: '/',
      method: 'GET'
    };

    var req = https.request(options, function(res) {
      console.log("statusCode: ", res.statusCode);
      console.log("headers: ", res.headers);

      res.on('data', function(d) {
        process.stdout.write(d);
      });
    });
    req.end();

    req.on('error', function(e) {
      console.error(e);
    });

The options argument has the following options

- host: IP or domain of host to make request to. Defaults to `'localhost'`.
- port: port of host to request to. Defaults to 443.
- path: Path to request. Default `'/'`.
- method: HTTP request method. Default `'GET'`.

- `host`: A domain name or IP address of the server to issue the request to.
  Defaults to `'localhost'`.
- `hostname`: To support `url.parse()` `hostname` is preferred over `host`
- `port`: Port of remote server. Defaults to 443.
- `method`: A string specifying the HTTP request method. Defaults to `'GET'`.
- `path`: Request path. Defaults to `'/'`. Should include query string if any.
  E.G. `'/index.html?page=12'`
- `headers`: An object containing request headers.
- `auth`: Basic authentication i.e. `'user:password'` to compute an
  Authorization header.
- `agent`: Controls [Agent](#https.Agent) behavior. When an Agent is
  used request will default to `Connection: keep-alive`. Possible values:
 - `undefined` (default): use [globalAgent](#https.globalAgent) for this
   host and port.
 - `Agent` object: explicitly use the passed in `Agent`.
 - `false`: opts out of connection pooling with an Agent, defaults request to
   `Connection: close`.

The following options from [tls.connect()](tls.html#tls.connect) can also be
specified. However, a [globalAgent](#https.globalAgent) silently ignores these.

- `key`: Private key to use for SSL. Default `null`.
- `passphrase`: A string of passphrase for the private key. Default `null`.
- `cert`: Public x509 certificate to use. Default `null`.
- `ca`: An authority certificate or array of authority certificates to check
  the remote host against.

In order to specify these options, use a custom `Agent`.

Example:

    var options = {
      host: 'encrypted.google.com',
      port: 443,
      path: '/',
      method: 'GET',
      key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
      cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem')
    };
    options.agent = new https.Agent(options);

    var req = https.request(options, function(res) {
      ...
    }

Or does not use an `Agent`.

Example:

    var options = {
      host: 'encrypted.google.com',
      port: 443,
      path: '/',
      method: 'GET',
      key: fs.readFileSync('test/fixtures/keys/agent2-key.pem'),
      cert: fs.readFileSync('test/fixtures/keys/agent2-cert.pem'),
      agent: false
    };

    var req = https.request(options, function(res) {
      ...
    }

## https.get(options, callback)

Like `http.get()` but for HTTPS.

Example:

    var https = require('https');

    https.get({ host: 'encrypted.google.com', path: '/' }, function(res) {
      console.log("statusCode: ", res.statusCode);
      console.log("headers: ", res.headers);

      res.on('data', function(d) {
        process.stdout.write(d);
      });

    }).on('error', function(e) {
      console.error(e);
    });


## Class: https.Agent

An Agent object for HTTPS similar to [http.Agent](http.html#http.Agent).
See [https.request()](#https.request) for more information.


## https.globalAgent

Global instance of [https.Agent](#https.Agent) which is used as the default
for all HTTPS client requests.
