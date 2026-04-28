'use strict';

const { createServer } = require('http2');
const { mustCall } = require('../common');

const server = createServer();
server.on('stream', (stream) => {
  // writableObjectMode and writableNeedDrain should be defined (boolean)
  console.log('writableObjectMode:', stream.writableObjectMode);
  console.log('writableNeedDrain:', stream.writableNeedDrain);
  if (typeof stream.writableObjectMode !== 'boolean') {
    throw new Error(`writableObjectMode should be boolean, got ${stream.writableObjectMode}`);
  }
  if (typeof stream.writableNeedDrain !== 'boolean') {
    throw new Error(`writableNeedDrain should be boolean, got ${stream.writableNeedDrain}`);
  }
  stream.respond();
  stream.end('ok');
});
server.listen(0, mustCall(() => {
  const client = require('http2').connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.on('response', () => {
    req.on('data', () => {});
    req.on('end', () => {
      client.close();
      server.close();
      console.log('PASS: writableObjectMode and writableNeedDrain are booleans');
    });
  });
}));
