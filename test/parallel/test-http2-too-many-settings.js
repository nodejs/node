'use strict';

// Tests that attempting to send too many non-acknowledged
// settings frames will result in an error

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const maxOutstandingSettings = 2;

function doTest(session) {
  session.on('error', common.expectsError({
    code: 'ERR_HTTP2_MAX_PENDING_SETTINGS_ACK',
    type: Error
  }));
  for (let n = 0; n < maxOutstandingSettings; n++) {
    session.settings({ enablePush: false });
    assert.strictEqual(session.pendingSettingsAck, true);
  }
}

{
  const server = h2.createServer({ maxOutstandingSettings });
  server.on('stream', common.mustNotCall());
  server.once('session', common.mustCall((session) => doTest(session)));

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    // On some operating systems, an ECONNRESET error may be emitted.
    // On others it won't be. Do not make this a mustCall
    client.on('error', () => {});
    client.on('close', common.mustCall(() => server.close()));
  }));
}

{
  const server = h2.createServer();
  server.on('stream', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`,
                              { maxOutstandingSettings });
    client.on('connect', () => doTest(client));
    client.on('close', () => server.close());
  }));
}
