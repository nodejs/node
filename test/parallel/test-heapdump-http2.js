// Flags: --expose-internals
'use strict';
const common = require('../common');
const { recordState } = require('../common/heap');
const http2 = require('http2');
if (!common.hasCrypto)
  common.skip('missing crypto');

{
  const state = recordState();
  state.validateSnapshotNodes('Http2Session', []);
  state.validateSnapshotNodes('Http2Stream', []);
}

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respondWithFile(__filename);
});
server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall(() => {
    const state = recordState();

    // `Node / Http2Stream` (C++) -> Http2Stream (JS)
    state.validateSnapshotNodes('Http2Stream', [
      {
        children: [
          { name: 'Http2Stream' }
        ]
      },
    ], { loose: true });

    // `Node / FileHandle` (C++) -> FileHandle (JS)
    state.validateSnapshotNodes('FileHandle', [
      {
        children: [
          { name: 'FileHandle' }
        ]
      }
    ]);
    state.validateSnapshotNodes('TCPSocketWrap', [
      {
        children: [
          { name: 'TCP' }
        ]
      }
    ], { loose: true });
    state.validateSnapshotNodes('TCPServerWrap', [
      {
        children: [
          { name: 'TCP' }
        ]
      }
    ], { loose: true });
    // `Node / StreamPipe` (C++) -> StreamPipe (JS)
    state.validateSnapshotNodes('StreamPipe', [
      {
        children: [
          { name: 'StreamPipe' }
        ]
      }
    ]);
    // `Node / Http2Session` (C++) -> Http2Session (JS)
    state.validateSnapshotNodes('Http2Session', [
      {
        children: [
          { name: 'Http2Session' },
          { name: 'streams' }
        ]
      }
    ], { loose: true });
  }));

  req.resume();
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
});
