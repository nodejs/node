// Flags: --expose-http2
'use strict';

// Tests http2.connect()

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fs = require('fs');
const h2 = require('http2');
const path = require('path');
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
      [{ port: port }, { protocol: 'http:' }],
      [{ port: port, hostname: '127.0.0.1' }, { protocol: 'http:' }]
    ];

    let count = items.length;

    const maybeClose = common.mustCall((client) => {
      client.destroy();
      if (--count === 0) {
        setImmediate(() => server.close());
      }
    }, items.length);

    items.forEach((i) => {
      const client =
        h2.connect.apply(null, i)
          .on('connect', common.mustCall(() => maybeClose(client)));
    });

    // Will fail because protocol does not match the server.
    h2.connect({ port: port, protocol: 'https:' })
      .on('socketError', common.mustCall());
  }));
}


{

  const options = {
    key: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-key.pem')),
    cert: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-cert.pem'))
  };

  const server = h2.createSecureServer(options);
  server.listen(0);

  server.on('listening', common.mustCall(function() {
    const port = this.address().port;

    const opts = { rejectUnauthorized: false };

    const items = [
      [`https://localhost:${port}`, opts],
      [new URL(`https://localhost:${port}`), opts],
      [url.parse(`https://localhost:${port}`), opts],
      [{ port: port, protocol: 'https:' }, opts],
      [{ port: port, hostname: '127.0.0.1', protocol: 'https:' }, opts]
    ];

    let count = items.length;

    const maybeClose = common.mustCall((client) => {
      client.destroy();
      if (--count === 0) {
        setImmediate(() => server.close());
      }
    }, items.length);

    items.forEach((i) => {
      const client =
        h2.connect.apply(null, i)
          .on('connect', common.mustCall(() => maybeClose(client)));
    });
  }));
}
