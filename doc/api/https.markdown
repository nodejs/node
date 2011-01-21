## HTTPS

HTTPS is the HTTP protocol over TLS/SSL. In Node this is implemented as a
separate module.

## https.Server
## https.createServer

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
Similar options to `http.request()`.

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




