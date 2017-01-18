'use strict';
const common = require('../common');
const assert = require('assert');

const util = require('util');
const net = require('net');
const fs = require('fs');
const crypto = require('crypto');

var keyPem = fs.readFileSync(common.fixturesDir + '/cert.pem');
var certPem = fs.readFileSync(common.fixturesDir + '/cert.pem');

try {
  var credentials = crypto.createCredentials({key: keyPem, cert: certPem});
} catch (e) {
  common.skip('node compiled without OpenSSL.');
  return;
}
var i = 0;
var server = net.createServer(function(connection) {
  connection.setSecure(credentials);
  connection.setEncoding('latin1');

  connection.on('secure', function() {
    //console.log('Secure');
  });

  connection.on('data', function(chunk) {
    console.log('recved: ' + JSON.stringify(chunk));
    connection.write('HTTP/1.0 200 OK\r\n' +
                     'Content-type: text/plain\r\n' +
                     'Content-length: 9\r\n' +
                     '\r\n' +
                     'OK : ' + i +
                     '\r\n\r\n');
    i = i + 1;
    connection.end();
  });

  connection.on('end', function() {
    connection.end();
  });

});
server.listen(4443);


