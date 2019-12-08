'use strict';

// Tests http2.connect()

const common = require('../common');
const Countdown = require('../common/countdown');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const h2 = require('http2');
const url = require('url');
const URL = url.URL;

{
  const server = h2.createServer();
  server.listen(0);

  server.on('listening', common.mustCall(function() {
    const port = this.address().port;

    const items = [
      [`http://localhost:${port}`],
      [new URL(`http://localhost:${port}`)],
      [url.parse(`http://localhost:${port}`)],
      [{ port }, { protocol: 'http:' }],
      [{ port, hostname: '127.0.0.1' }, { protocol: 'http:' }]
    ];

    const serverClose = new Countdown(items.length + 1,
                                      () => setImmediate(() => server.close()));

    const maybeClose = common.mustCall((client) => {
      client.close();
      serverClose.dec();
    }, items.length);

    items.forEach((i) => {
      const client =
        h2.connect.apply(null, i)
          .on('connect', common.mustCall(() => maybeClose(client)));
      client.on('close', common.mustCall());
    });

    // Will fail because protocol does not match the server.
    const client = h2.connect({ port: port, protocol: 'https:' })
      .on('error', common.mustCall(() => serverClose.dec()));
    client.on('close', common.mustCall());
  }));
}


{

  const options = {
    key: fixtures.readKey('agent3-key.pem'),
    cert: fixtures.readKey('agent3-cert.pem')
  };

  const server = h2.createSecureServer(options);
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;

    const opts = { rejectUnauthorized: false };

    const items = [
      [`https://localhost:${port}`, opts],
      [new URL(`https://localhost:${port}`), opts],
      [url.parse(`https://localhost:${port}`), opts],
      [{ port: port, protocol: 'https:' }, opts],
      [{ port: port, hostname: '127.0.0.1', protocol: 'https:' }, opts]
    ];

    const serverClose = new Countdown(items.length,
                                      () => setImmediate(() => server.close()));

    const maybeClose = common.mustCall((client) => {
      client.close();
      serverClose.dec();
    }, items.length);

    items.forEach((i) => {
      const client =
        h2.connect.apply(null, i)
          .on('connect', common.mustCall(() => maybeClose(client)));
    });
  }));
}
