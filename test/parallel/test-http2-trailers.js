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

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.on('trailers', common.mustCall((headers) => {
    assert.strictEqual(headers[trailerKey], trailerValue);
    stream.end(body);
  }));
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  }, { waitForTrailers: true });
  stream.on('wantTrailers', () => {
    stream.sendTrailers({ [trailerKey]: trailerValue });
    assert.throws(
      () => stream.sendTrailers({}),
      {
        code: 'ERR_HTTP2_TRAILERS_ALREADY_SENT',
        name: 'Error'
      }
    );
  });

  assert.throws(
    () => stream.sendTrailers({}),
    {
      code: 'ERR_HTTP2_TRAILERS_NOT_READY',
      name: 'Error'
    }
  );
}

server.listen(0);

server.on('listening', common.mustCall(function() {
  const client = h2.connect(`http://localhost:${this.address().port}`);
  const req = client.request({ ':path': '/', ':method': 'POST' },
                             { waitForTrailers: true });
  req.on('wantTrailers', () => {
    req.sendTrailers({ [trailerKey]: trailerValue });
  });
  req.on('data', common.mustCall());
  req.on('trailers', common.mustCall((headers) => {
    assert.strictEqual(headers[trailerKey], trailerValue);
  }));
  req.on('close', common.mustCall(() => {
    assert.throws(
      () => req.sendTrailers({}),
      {
        code: 'ERR_HTTP2_INVALID_STREAM',
        name: 'Error'
      }
    );
    server.close();
    client.close();
  }));
  req.end('data');

}));
