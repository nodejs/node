'use strict';
// This tests heap snapshot integration of SecureContext.

const common = require('../common');

if (!common.hasCrypto) common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const {
  createJSHeapSnapshot,
  validateByRetainingPathFromNodes,
} = require('../common/heap');
const tls = require('tls');


// eslint-disable-next-line no-unused-vars
const ctx = tls.createSecureContext({
  cert: fixtures.readKey('agent1-cert.pem'),
  key: fixtures.readKey('agent1-key.pem'),
});

{
  const nodes = createJSHeapSnapshot();
  validateByRetainingPathFromNodes(nodes, 'Node / SecureContext', [
    { edge_name: 'ctx', node_name: 'Node / ctx' },
  ]);
  validateByRetainingPathFromNodes(nodes, 'Node / SecureContext', [
    { edge_name: 'cert', node_name: 'Node / cert' },
  ]);
}
