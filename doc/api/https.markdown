# HTTPS

    Stability: 3 - Stable

HTTPS is the HTTP protocol over TLS/SSL. In Node this is implemented as a
separate module.

## Class: https.Server

This class is a subclass of `tls.Server` and emits events same as
`http.Server`. See `http.Server` for more information.

## https.createServer(options, [requestListener])

Returns a new HTTPS web server object. The `options` is similar to
[tls.createServer()][].  The `requestListener` is a function which is
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

Or

    var https = require('https');
    var fs = require('fs');

    var options = {
      pfx: fs.readFileSync('server.pfx')
    };

    https.createServer(options, function (req, res) {
      res.writeHead(200);
      res.end("hello world\n");
    }).listen(8000);


### server.listen(port, [host], [backlog], [callback])
### server.listen(path, [callback])
### server.listen(handle, [callback])

See [http.listen()][] for details.

### server.close([callback])

See [http.close()][] for details.

## https.request(options, callback)

Makes a request to a secure web server.

`options` can be an object or a string. If `options` is a string, it is
automatically parsed with [url.parse()](url.html#url.parse).

All options from [http.request()][] are valid.

Example:

    var https = require('https');

    var options = {
      hostname: 'encrypted.google.com',
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
- `agent`: Controls [Agent][] behavior. When an Agent is used request will
  default to `Connection: keep-alive`. Possible values:
 - `undefined` (default): use [globalAgent][] for this host and port.
 - `Agent` object: explicitly use the passed in `Agent`.
 - `false`: opts out of connection pooling with an Agent, defaults request to
   `Connection: close`.

The following options from [tls.connect()][] can also be specified. However, a
[globalAgent][] silently ignores these.

- `pfx`: Certificate, Private key and CA certificates to use for SSL. Default `null`.
- `key`: Private key to use for SSL. Default `null`.
- `passphrase`: A string of passphrase for the private key or pfx. Default `null`.
- `cert`: Public x509 certificate to use. Default `null`.
- `ca`: An authority certificate or array of authority certificates to check
  the remote host against.
- `ciphers`: A string describing the ciphers to use or exclude. Consult
  <http://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT> for
  details on the format.
- `rejectUnauthorized`: If `true`, the server certificate is verified against
  the list of supplied CAs. An `'error'` event is emitted if verification
  fails. Verification happens at the connection level, *before* the HTTP
  request is sent. Default `true`.

In order to specify these options, use a custom `Agent`.

Example:

    var options = {
      hostname: 'encrypted.google.com',
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
      hostname: 'encrypted.google.com',
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

`options` can be an object or a string. If `options` is a string, it is
automatically parsed with [url.parse()](url.html#url.parse).

Example:

    var https = require('https');

    https.get('https://encrypted.google.com/', function(res) {
      console.log("statusCode: ", res.statusCode);
      console.log("headers: ", res.headers);

      res.on('data', function(d) {
        process.stdout.write(d);
      });

    }).on('error', function(e) {
      console.error(e);
    });


## Class: https.Agent

An Agent object for HTTPS similar to [http.Agent][].  See [https.request()][]
for more information.


## https.globalAgent

Global instance of [https.Agent][] for all HTTPS client requests.

[Agent]: #https_class_https_agent
[globalAgent]: #https_https_globalagent
[http.listen()]: http.html#http_server_listen_port_hostname_backlog_callback
[http.close()]: http.html#http_server_close_callback
[http.Agent]: http.html#http_class_http_agent
[http.request()]: http.html#http_http_request_options_callback
[https.Agent]: #https_class_https_agent
[https.request()]: #https_https_request_options_callback
[tls.connect()]: tls.html#tls_tls_connect_options_callback
[tls.createServer()]: tls.html#tls_tls_createserver_options_secureconnectionlistener
