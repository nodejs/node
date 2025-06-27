'use strict';
const common = require('../common');

// Before https://github.com/nodejs/node/pull/2847 a child process trying
// (asynchronously) to use the closed channel to it's creator caused a segfault.

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (!cluster.isPrimary) {
  // Exit on first received handle to leave the queue non-empty in primary
  process.on('message', function() {
    process.exit(1);
  });
  return;
}

const server = net
  .createServer(function(s) {
    if (common.isWindows) {
      s.on('error', function(err) {
        // Prevent possible ECONNRESET errors from popping up
        if (err.code !== 'ECONNRESET') throw err;
      });
    }
    setTimeout(function() {
      s.destroy();
    }, 100);
  })
  .listen(0, function() {
    const worker = cluster.fork();

    worker.on('error', function(err) {
      if (
        err.code !== 'ECONNRESET' &&
        err.code !== 'ECONNREFUSED' &&
        err.code !== 'EMFILE'
      ) {
        throw err;
      }
    });

    function send(callback) {
      const s = net.connect(server.address().port, function() {
        worker.send({}, s, callback);
      });

      // https://github.com/nodejs/node/issues/3635#issuecomment-157714683
      // ECONNREFUSED or ECONNRESET errors can happen if this connection is
      // still establishing while the server has already closed.
      // EMFILE can happen if the worker __and__ the server had already closed.
      s.on('error', function(err) {
        if (
          err.code !== 'ECONNRESET' &&
          err.code !== 'ECONNREFUSED' &&
          err.code !== 'EMFILE'
        ) {
          throw err;
        }
      });
    }

    worker.process.once(
      'close',
      common.mustCall(function() {
        // Otherwise the crash on `channel.fd` access may happen
        assert.strictEqual(worker.process.channel, null);
        server.close();
      })
    );

    worker.on('online', function() {
      send(function(err) {
        assert.ifError(err);
        send(function(err) {
          // Ignore errors when sending the second handle because the worker
          // may already have exited.
          if (err && err.code !== 'ERR_IPC_CHANNEL_CLOSED' &&
                     err.code !== 'ECONNRESET' &&
                     err.code !== 'ECONNREFUSED' &&
                     err.code !== 'EMFILE') {
            throw err;
          }
        });
      });
    });
  });
