'use strict';

// This tests heap snapshot integration of http2.

const common = require('../common');
const { createJSHeapSnapshot, validateByRetainingPath, validateByRetainingPathFromNodes } = require('../common/heap');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Before http2 is used, no Http2Session or Http2Streamshould be created.
{
  const sessions = validateByRetainingPath('Node / Http2Session', []);
  assert.strictEqual(sessions.length, 0);
  const streams = validateByRetainingPath('Node / Http2Stream', []);
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
    validateByRetainingPathFromNodes(nodes, 'Node / Http2Stream', [
      // current_headers and/or queue could be empty
      { node_name: 'Http2Stream', edge_name: 'native_to_javascript' },
    ]);

    // `Node / FileHandle` (C++) -> FileHandle (JS)
    validateByRetainingPathFromNodes(nodes, 'Node / FileHandle', [
      { node_name: 'FileHandle', edge_name: 'native_to_javascript' },
      // current_headers could be empty
    ]);
    validateByRetainingPathFromNodes(nodes, 'Node / TCPSocketWrap', [
      { node_name: 'TCP', edge_name: 'native_to_javascript' },
    ]);

    validateByRetainingPathFromNodes(nodes, 'Node / TCPServerWrap', [
      { node_name: 'TCP', edge_name: 'native_to_javascript' },
    ]);

    // We don't necessarily have Node / StreamPipe here because by the time the
    // response event is emitted, the file may have already been fully piped here
    // and the stream pipe may have been destroyed.

    // `Node / Http2Session` (C++) -> Http2Session (JS)
    const sessions = validateByRetainingPathFromNodes(nodes, 'Node / Http2Session', []);
    validateByRetainingPathFromNodes(sessions, 'Node / Http2Session', [
      { node_name: 'Http2Session', edge_name: 'native_to_javascript' },
    ]);
    validateByRetainingPathFromNodes(sessions, 'Node / Http2Session', [
      { node_name: 'Node / nghttp2_memory', edge_name: 'nghttp2_memory' },
    ]);
    validateByRetainingPathFromNodes(sessions, 'Node / Http2Session', [
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
