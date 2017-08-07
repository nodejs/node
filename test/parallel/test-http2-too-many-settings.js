// Flags: --expose-http2
'use strict';

// Tests that attempting to send too many non-acknowledged
// settings frames will result in a throw.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const maxPendingAck = 2;
const server = h2.createServer({ maxPendingAck: maxPendingAck + 1 });

let clients = 2;

function doTest(session) {
  for (let n = 0; n < maxPendingAck; n++)
    assert.doesNotThrow(() => session.settings({ enablePush: false }));
  assert.throws(() => session.settings({ enablePush: false }),
                common.expectsError({
                  code: 'ERR_HTTP2_MAX_PENDING_SETTINGS_ACK',
                  type: Error
                }));
}

server.on('stream', common.mustNotCall());

server.once('session', common.mustCall((session) => doTest(session)));

server.listen(0);

const closeServer = common.mustCall(() => {
  if (--clients === 0)
    server.close();
}, clients);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`,
                            { maxPendingAck: maxPendingAck + 1 });
  let remaining = maxPendingAck + 1;

  client.on('close', closeServer);
  client.on('localSettings', common.mustCall(() => {
    if (--remaining <= 0) {
      client.destroy();
    }
  }, maxPendingAck + 1));
  client.on('connect', common.mustCall(() => doTest(client)));
}));

// Setting maxPendingAck to 0, defaults it to 1
server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`,
                            { maxPendingAck: 0 });

  client.on('close', closeServer);
  client.on('localSettings', common.mustCall(() => {
    client.destroy();
  }));
}));
