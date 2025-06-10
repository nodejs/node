// Flags: --expose-internals
'use strict';
const common = require('../common');
const { recordState } = require('../common/heap');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

{
  const state = recordState();
  state.validateSnapshotNodes('Node / Http2Session', []);
  state.validateSnapshotNodes('Node / Http2Stream', []);
}

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respondWithFile(process.execPath);
});
server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall(() => {
    const state = recordState();

    // `Node / Http2Stream` (C++) -> Http2Stream (JS)
    state.validateSnapshotNodes('Node / Http2Stream', [
      {
        children: [
          // current_headers and/or queue could be empty
          { node_name: 'Http2Stream', edge_name: 'native_to_javascript' },
        ],
      },
    ], { loose: true });

    // `Node / FileHandle` (C++) -> FileHandle (JS)
    state.validateSnapshotNodes('Node / FileHandle', [
      {
        children: [
          { node_name: 'FileHandle', edge_name: 'native_to_javascript' },
          // current_headers could be empty
        ],
      },
    ], { loose: true });
    state.validateSnapshotNodes('Node / TCPSocketWrap', [
      {
        children: [
          { node_name: 'TCP', edge_name: 'native_to_javascript' },
        ],
      },
    ], { loose: true });
    state.validateSnapshotNodes('Node / TCPServerWrap', [
      {
        children: [
          { node_name: 'TCP', edge_name: 'native_to_javascript' },
        ],
      },
    ], { loose: true });

    // We don't necessarily have Node / StreamPipe here because by the time the
    // response event is emitted, the file may have already been fully piped here
    // and the stream pipe may have been destroyed.

    // `Node / Http2Session` (C++) -> Http2Session (JS)
    state.validateSnapshotNodes('Node / Http2Session', [
      {
        children: [
          { node_name: 'Http2Session', edge_name: 'native_to_javascript' },
          { node_name: 'Node / nghttp2_memory', edge_name: 'nghttp2_memory' },
          {
            node_name: 'Node / streams', edge_name: 'streams',
          },
          // outstanding_pings, outgoing_buffers, outgoing_storage,
          // pending_rst_streams could be empty
        ],
      },
    ], { loose: true });
  }));

  req.resume();
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
});
