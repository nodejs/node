'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const MakeDuplexPair = require('../common/duplexpair');

// Regression test for the crash reported in
// https://github.com/nodejs/node/issues/15102 (httpParser.finish() is called
// during httpParser.execute()):

{
  const { clientSide, serverSide } = MakeDuplexPair();

  serverSide.on('data', common.mustCall((data) => {
    assert.strictEqual(data.toString('utf8'), `\
GET / HTTP/1.1
Host: localhost:80
Connection: close

`.replace(/\n/g, '\r\n'));

    setImmediate(() => {
      serverSide.write('HTTP/1.1 200 OK\r\n\r\n');
    });
  }));

  const req = http.get({
    createConnection: common.mustCall(() => clientSide)
  }, common.mustCall((res) => {
    req.on('error', common.mustCall());
    req.socket.emit('error', new Error('very fake error'));
  }));
  req.end();
}
