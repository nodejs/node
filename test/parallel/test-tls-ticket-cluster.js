'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var cluster = require('cluster');
var fs = require('fs');
var join = require('path').join;

var workerCount = 4;
var expectedReqCount = 16;

if (cluster.isMaster) {
  var reusedCount = 0;
  var reqCount = 0;
  var lastSession = null;
  var shootOnce = false;

  function shoot() {
    console.error('[master] connecting');
    var c = tls.connect(common.PORT, {
      session: lastSession,
      rejectUnauthorized: false
    }, function() {
      lastSession = c.getSession();
      c.end();

      if (++reqCount === expectedReqCount) {
        Object.keys(cluster.workers).forEach(function(id) {
          cluster.workers[id].send('die');
        });
      } else {
        shoot();
      }
    });
  }

  function fork() {
    var worker = cluster.fork();
    worker.on('message', function(msg) {
      console.error('[master] got %j', msg);
      if (msg === 'reused') {
        ++reusedCount;
      } else if (msg === 'listening' && !shootOnce) {
        shootOnce = true;
        shoot();
      }
    });

    worker.on('exit', function() {
      console.error('[master] worker died');
    });
  }
  for (var i = 0; i < workerCount; i++) {
    fork();
  }

  process.on('exit', function() {
    assert.equal(reqCount, expectedReqCount);
    assert.equal(reusedCount + 1, reqCount);
  });
  return;
}

var keyFile = join(common.fixturesDir, 'agent.key');
var certFile = join(common.fixturesDir, 'agent.crt');
var key = fs.readFileSync(keyFile);
var cert = fs.readFileSync(certFile);
var options = {
  key: key,
  cert: cert
};

var server = tls.createServer(options, function(c) {
  if (c.isSessionReused()) {
    process.send('reused');
  } else {
    process.send('not-reused');
  }
  c.end();
});

server.listen(common.PORT, function() {
  process.send('listening');
});

process.on('message', function listener(msg) {
  console.error('[worker] got %j', msg);
  if (msg === 'die') {
    server.close(function() {
      console.error('[worker] server close');

      process.exit();
    });
  }
});

process.on('exit', function() {
  console.error('[worker] exit');
});
