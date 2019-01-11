'use strict';

const common = require('../common');
const domain = require('domain');
const http = require('http');

const agent = new http.Agent({
  // keepAlive is important here so that the underlying socket of all requests
  // is reused and tied to the same domain.
  keepAlive: true
});

const reqSockets = new Set();

function performHttpRequest(opts, cb) {
  const req = http.get({ port: server.address().port, agent }, (res) => {
    if (!req.socket) {
      console.log('missing socket property on req object, failing test');
      markTestAsFailed();
      req.abort();
      cb(new Error('req.socket missing'));
      return;
    }

    reqSockets.add(req.socket);

    // Consume the data from the response to make sure the 'end' event is
    // emitted.
    res.on('data', function noop() {});
    res.on('end', () => {
      if (opts.shouldThrow) {
        throw new Error('boom');
      } else {
        cb();
      }
    });

    res.on('error', (resErr) => {
      console.log('got error on response, failing test:', resErr);
      markTestAsFailed();
    });

  }).on('error', (reqErr) => {
    console.log('got error on request, failing test:', reqErr);
    markTestAsFailed();
  });

  req.end();
}

function performHttpRequestInNewDomain(opts, cb) {
  const d = domain.create();
  d.run(() => {
    performHttpRequest(opts, cb);
  });

  return d;
}

const server = http.createServer((req, res) => {
  res.end();
});

server.listen(0, common.mustCall(function() {
  const d1 = performHttpRequestInNewDomain({
    shouldThrow: false
  }, common.mustCall((firstReqErr) => {
    if (firstReqErr) {
      markTestAsFailed();
      return;
    }
    // We want to schedule the second request on the next turn of the event
    // loop so that the parser from the first request is actually reused.
    setImmediate(common.mustCall(() => {
      const d2 = performHttpRequestInNewDomain({
        shouldThrow: true
      }, (secondReqErr) => {
        // Since this request throws before its callback has the chance
        // to be called, we mark the test as failed if this callback is
        // called.
        console.log('second request\'s cb called unexpectedly, failing test');
        markTestAsFailed();
      });

      // The second request throws when its response's end event is
      // emitted. So we expect its domain to emit an error event.
      d2.on('error', common.mustCall((d2err) => {
        server.close();
        if (reqSockets.size !== 1) {
          console.log('more than 1 socket used for both requests, failing ' +
            'test');
          markTestAsFailed();
        }
      }, 1));
    }));
  }));

  d1.on('error', (d1err) => {
    // d1 is the domain attached to the first request, which doesn't throw,
    // so we don't expect its error handler to be called.
    console.log('first request\'s domain\'s error handler called, ' +
      'failing test');
    markTestAsFailed();
  });
}));

function markTestAsFailed() {
  if (server) {
    server.close();
  }
  process.exitCode = 1;
}
