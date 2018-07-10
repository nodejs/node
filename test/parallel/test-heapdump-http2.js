// Flags: --expose-internals
'use strict';
const common = require('../common');
const { recordState } = require('../common/heap');
const http2 = require('http2');
if (!common.hasCrypto)
  common.skip('missing crypto');

{
  const state = recordState();
  state.validateSnapshotNodes('HTTP2SESSION', []);
  state.validateSnapshotNodes('HTTP2STREAM', []);
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
    state.validateSnapshotNodes('HTTP2STREAM', [
      {
        children: [
          { name: 'Http2Stream' }
        ]
      },
    ], { loose: true });
    state.validateSnapshotNodes('FILEHANDLE', [
      {
        children: [
          { name: 'FileHandle' }
        ]
      }
    ]);
    state.validateSnapshotNodes('TCPWRAP', [
      {
        children: [
          { name: 'TCP' }
        ]
      }
    ], { loose: true });
    state.validateSnapshotNodes('TCPSERVERWRAP', [
      {
        children: [
          { name: 'TCP' }
        ]
      }
    ], { loose: true });
    state.validateSnapshotNodes('STREAMPIPE', [
      {
        children: [
          { name: 'StreamPipe' }
        ]
      }
    ]);
    state.validateSnapshotNodes('HTTP2SESSION', [
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
