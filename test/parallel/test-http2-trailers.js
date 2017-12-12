'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';
const trailerKey = 'test-trailer';
const trailerValue = 'testing';

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.on('trailers', common.mustCall((headers) => {
    assert.strictEqual(headers[trailerKey], trailerValue);
  }));
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  }, {
    getTrailers: common.mustCall((trailers) => {
      trailers[trailerKey] = trailerValue;
    })
  });
  stream.end(body);
}

server.listen(0);

server.on('listening', common.mustCall(function() {
  const client = h2.connect(`http://localhost:${this.address().port}`);
  const req = client.request({ ':path': '/', ':method': 'POST' }, {
    getTrailers: common.mustCall((trailers) => {
      trailers[trailerKey] = trailerValue;
    })
  });
  req.on('data', common.mustCall());
  req.on('trailers', common.mustCall((headers) => {
    assert.strictEqual(headers[trailerKey], trailerValue);
  }));
  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
  req.end('data');

}));
