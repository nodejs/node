'use strict';

// This tests heap snapshot integration of http2.

const common = require('../common');
const { createJSHeapSnapshot, findByRetainingPath, findByRetainingPathFromNodes } = require('../common/heap');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Before http2 is used, no Http2Session or Http2Streamshould be created.
{
  const sessions = findByRetainingPath('Node / Http2Session', []);
  assert.strictEqual(sessions.length, 0);
  const streams = findByRetainingPath('Node / Http2Stream', []);
  assert.strictEqual(streams.length, 0);
}

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respondWithFile(process.execPath);
});
server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall(() => {
    const nodes = createJSHeapSnapshot();

    // `Node / Http2Stream` (C++) -> Http2Stream (JS)
    findByRetainingPathFromNodes(nodes, 'Node / Http2Stream', [
      // current_headers and/or queue could be empty
      { node_name: 'Http2Stream', edge_name: 'native_to_javascript' },
    ]);

    // `Node / FileHandle` (C++) -> FileHandle (JS)
    findByRetainingPathFromNodes(nodes, 'Node / FileHandle', [
      { node_name: 'FileHandle', edge_name: 'native_to_javascript' },
      // current_headers could be empty
    ]);
    findByRetainingPathFromNodes(nodes, 'Node / TCPSocketWrap', [
      { node_name: 'TCP', edge_name: 'native_to_javascript' },
    ]);

    findByRetainingPathFromNodes(nodes, 'Node / TCPServerWrap', [
      { node_name: 'TCP', edge_name: 'native_to_javascript' },
    ]);

    // `Node / StreamPipe` (C++) -> StreamPipe (JS)
    findByRetainingPathFromNodes(nodes, 'Node / StreamPipe', [
      { node_name: 'StreamPipe', edge_name: 'native_to_javascript' },
    ]);

    // `Node / Http2Session` (C++) -> Http2Session (JS)
    const sessions = findByRetainingPathFromNodes(nodes, 'Node / Http2Session', []);
    findByRetainingPathFromNodes(sessions, 'Node / Http2Session', [
      { node_name: 'Http2Session', edge_name: 'native_to_javascript' },
    ]);
    findByRetainingPathFromNodes(sessions, 'Node / Http2Session', [
      { node_name: 'Node / nghttp2_memory', edge_name: 'nghttp2_memory' },
    ]);
    findByRetainingPathFromNodes(sessions, 'Node / Http2Session', [
      { node_name: 'Node / streams', edge_name: 'streams' },
    ]);
    // outstanding_pings, outgoing_buffers, outgoing_storage,
    // pending_rst_streams could be empty
  }));

  req.resume();
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
});
