// Flags: --expose-internals
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { validateSnapshotNodes } = require('../common/heap');
const net = require('net');
const tls = require('tls');

validateSnapshotNodes('TLSWrap', []);

const server = net.createServer(common.mustCall((c) => {
  c.end();
})).listen(0, common.mustCall(() => {
  const c = tls.connect({ port: server.address().port });

  c.on('error', common.mustCall(() => {
    server.close();
  }));
  c.write('hello');

  validateSnapshotNodes('TLSWrap', [
    {
      children: [
        { name: 'NodeBIO' },
        { name: 'NodeBIO' },
        // `Node / TLSWrap` (C++) -> `TLSWrap` (JS)
        { name: 'TLSWrap' }
      ]
    }
  ]);
}));
