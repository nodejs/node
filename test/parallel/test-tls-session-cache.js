'use strict';
var common = require('../common');

if (!common.opensslCli) {
  console.log('1..0 # Skipped: node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

doTest({ tickets: false }, function() {
  doTest({ tickets: true }, function() {
    console.error('all done');
  });
});

function doTest(testOptions, callback) {
  var assert = require('assert');
  var tls = require('tls');
  var fs = require('fs');
  var join = require('path').join;
  var spawn = require('child_process').spawn;

  var keyFile = join(common.fixturesDir, 'agent.key');
  var certFile = join(common.fixturesDir, 'agent.crt');
  var key = fs.readFileSync(keyFile);
  var cert = fs.readFileSync(certFile);
  var options = {
    key: key,
    cert: cert,
    ca: [cert],
    requestCert: true
  };
  var requestCount = 0;
  var resumeCount = 0;
  var session;

  var server = tls.createServer(options, function(cleartext) {
    cleartext.on('error', function(er) {
      // We're ok with getting ECONNRESET in this test, but it's
      // timing-dependent, and thus unreliable. Any other errors
      // are just failures, though.
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    ++requestCount;
    cleartext.end();
  });
  server.on('newSession', function(id, data, cb) {
    // Emulate asynchronous store
    setTimeout(function() {
      assert.ok(!session);
      session = {
        id: id,
        data: data
      };
      cb();
    }, 1000);
  });
  server.on('resumeSession', function(id, callback) {
    ++resumeCount;
    assert.ok(session);
    assert.equal(session.id.toString('hex'), id.toString('hex'));

    // Just to check that async really works there
    setTimeout(function() {
      callback(null, session.data);
    }, 100);
  });

  var args = [
    's_client',
    '-tls1',
    '-connect', 'localhost:' + common.PORT,
    '-servername', 'ohgod',
    '-key', join(common.fixturesDir, 'agent.key'),
    '-cert', join(common.fixturesDir, 'agent.crt'),
    '-reconnect'
  ].concat(testOptions.tickets ? [] : '-no_ticket');

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  server.listen(common.PORT, function() {
    var client = spawn(common.opensslCli, args, {
      stdio: [ 0, 1, 'pipe' ]
    });
    var err = '';
    client.stderr.setEncoding('utf8');
    client.stderr.on('data', function(chunk) {
      err += chunk;
    });
    client.on('exit', function(code) {
      console.error('done');
      assert.equal(code, 0);
      server.close(function() {
        setTimeout(callback, 100);
      });
    });
  });

  process.on('exit', function() {
    if (testOptions.tickets) {
      assert.equal(requestCount, 6);
      assert.equal(resumeCount, 0);
    } else {
      // initial request + reconnect requests (5 times)
      assert.ok(session);
      assert.equal(requestCount, 6);
      assert.equal(resumeCount, 5);
    }
  });
}
