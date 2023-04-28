// Flags: --expose-internals
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { validateSnapshotNodes } = require('../common/heap');
const net = require('net');
const tls = require('tls');

validateSnapshotNodes('Node / TLSWrap', []);

const server = net.createServer(common.mustCall((c) => {
  c.end();
})).listen(0, common.mustCall(() => {
  const c = tls.connect({ port: server.address().port });

  c.on('error', common.mustCall(() => {
    server.close();
  }));
  c.write('hello');

  validateSnapshotNodes('Node / TLSWrap', [
    {
      children: [
        { node_name: 'Node / NodeBIO', edge_name: 'enc_out' },
        { node_name: 'Node / NodeBIO', edge_name: 'enc_in' },
        // `Node / TLSWrap` (C++) -> `TLSWrap` (JS)
        { node_name: 'TLSWrap', edge_name: 'native_to_javascript' },
        // pending_cleartext_input could be empty
      ],
    },
  ]);
}));
