'use strict';

const common = require('../common');
const domain = require('domain');
const http = require('http');

const agent = new http.Agent({
  // keepAlive is important here so that the underlying socket of all requests
  // is reused and tied to the same domain.
  keepAlive: true
});

function performHttpRequest(opts, cb) {
  const req = http.get({ port: server.address().port, agent }, (res) => {
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
      process.exit(1);
    });

  }).on('error', (reqErr) => {
    process.exit(1);
  });

  req.end();
}

function performHttpRequestInNewDomain(opts, cb) {
  const d = domain.create();
  d._id = opts.id;
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
    // We want to schedule the second request on the next turn of the event
    // loop so that the parser from the first request is actually reused.
    setImmediate(common.mustCall(() => {
      const d2 = performHttpRequestInNewDomain({
        shouldThrow: true
      }, (secondReqErr) => {
        // Since this request throws before its callback has the chance
        // to be called, we mark the test as failed if this callback is
        // called.
        process.exit(1);
      });

      // The second request throws when its response's end event is
      // emitted. So we expect its domain to emit an error event.
      d2.on('error', common.mustCall((d2err) => {
        server.close();
      }, 1));
    }));
  }));

  d1.on('error', (d1err) => {
    // d1 is the domain attached to the first request, which doesn't throw,
    // so we don't expect its error handler to be called.
    process.exit(1);
  });
}));
