'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

var assert = require('assert');
var path = require('path');
var fs = require('fs');
var spawn = require('child_process').spawn;
var tls = require('tls');

var key_path = path.join(common.fixturesDir, 'test_key.pem');
var cert_path = path.join(common.fixturesDir, 'test_cert.pem');
var ca_path = path.join(common.fixturesDir, 'test_ca.pem');
var serverOptions = {
  key: fs.readFileSync(key_path),
  cert: fs.readFileSync(cert_path),
  ca: [ fs.readFileSync(ca_path) ]
};

var numberOfTests = 5;
var testsFinished = 0;
var server;

function finishTest() {
  testsFinished++;
  if (testsFinished == numberOfTests) {
    server.close();
  }
}

function runTlsClient(args, callback) {
  var out = '';
  var err = '';

  var tlsClientProcess = spawn(process.execPath, args, {});
  tlsClientProcess.stderr.on('data', function(chunk) {
    err += chunk;
    process.stderr.write(chunk);
  });
  tlsClientProcess.stdout.on('data', function(chunk) {
    out += chunk;
  });
  tlsClientProcess.stdin.write(
    'var tls = require("tls"); ' +
    'var options = { ' +
    ' port: ' + common.PORT +
    '};' +
    'tls.connect(options).on("error", function(err) { ' +
       'console.log(err); ' +
       'this.destroy(); ' +
       'process.exit(1); ' +
    '}).on("data", function (data) { ' +
    '  console.log(data.toString()); ' +
    '  this.end(); ' +
    '}).write("123"); '
  );
  tlsClientProcess.stdin.end();

  tlsClientProcess.stdout.on('close', function() {
    finishTest();
    callback(err, out);
  });
}

var server = tls.createServer(serverOptions, function(socket) {
  socket.write('Hello');
}).listen(common.PORT, function() {

  // Check we can successfully connect when we specify our root certificates
  runTlsClient(['--root-certs', ca_path], function(err, out) {
    console.log(err);
    assert.equal(out, 'Hello\n');
  });

  // Check we get certificate errors back if we don't specify certificates
  // and thus end up with the defaults
  runTlsClient([], function(err, out) {
    console.log(err);
    assert.equal(out, '{ [Error: self signed certificate] code: ' +
        '\'DEPTH_ZERO_SELF_SIGNED_CERT\' }\n');
  });

  // Check node errors if the root certificate file is specified twice
  runTlsClient(['--root-certs', ca_path, '--root-certs', ca_path]
  , function(err, out) {
    console.log(out);
    assert.equal(err, process.execPath +
        ': --root-certs cannot be specified twice\n');
  });

  // Check node errors if certificate file specified doesn't exist
  runTlsClient(['--root-certs', 'this_file_should_definitely_not_exist']
  , function(err, out) {
    console.log(out);
    assert.equal(err, process.execPath + ': Unable to load root certs file ' +
        'this_file_should_definitely_not_exist\n');
  });

  // Check node errors if 2nd argument to --root-certs not specified
  runTlsClient(['--root-certs'], function(err, out) {
    console.log(out);
    assert.equal(err, process.execPath +
        ': --root-certs requires an argument\n');
  });
});
