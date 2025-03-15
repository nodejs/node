'use strict';
// This tests heap snapshot integration of tls sockets.
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { findByRetainingPath, findByRetainingPathFromNodes } = require('../common/heap');
const assert = require('assert');
const net = require('net');
const tls = require('tls');

// Before tls is used, no TLSWrap should be created.
{
  const nodes = findByRetainingPath('Node / TLSWrap', []);
  assert.strictEqual(nodes.length, 0);
}

const server = net.createServer(common.mustCall((c) => {
  c.end();
})).listen(0, common.mustCall(() => {
  const c = tls.connect({ port: server.address().port });

  c.on('error', common.mustCall(() => {
    server.close();
  }));
  c.write('hello');

  const nodes = findByRetainingPath('Node / TLSWrap', []);
  findByRetainingPathFromNodes(nodes, 'Node / TLSWrap', [
    { node_name: 'Node / NodeBIO', edge_name: 'enc_out' },
  ]);
  findByRetainingPathFromNodes(nodes, 'Node / TLSWrap', [
    { node_name: 'Node / NodeBIO', edge_name: 'enc_in' },
  ]);
  findByRetainingPathFromNodes(nodes, 'Node / TLSWrap', [
    // `Node / TLSWrap` (C++) -> `TLSWrap` (JS)
    { node_name: 'TLSWrap', edge_name: 'native_to_javascript' },
    // pending_cleartext_input could be empty
  ]);
}));
