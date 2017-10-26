'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();
const invalidOptValueError = (value) => ({
  type: TypeError,
  code: 'ERR_INVALID_OPT_VALUE',
  message: `The value "${value}" is invalid for option "parent"`
});

// we use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  common.expectsError(
    () => stream.priority({
      parent: stream.id,
      weight: 1,
      exclusive: false
    }),
    invalidOptValueError(stream.id)
  );
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('hello world');
}));

server.listen(0, common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  req.on(
    'ready',
    () => common.expectsError(
      () => req.priority({
        parent: req.id,
        weight: 1,
        exclusive: false
      }),
      invalidOptValueError(req.id)
    )
  );

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();

}));
