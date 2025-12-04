'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const expected = {
  '__proto__': null,
  'testheader1': 'foo',
  'testheader2': 'bar',
  'testheader3': 'xyz'
};
const server = http.createServer(common.mustCall((req, res) => {
  let retval = res.setHeader('testheader1', 'foo');

  // Test that the setHeader returns the same response object.
  assert.strictEqual(retval, res);

  retval = res.setHeader('testheader2', 'bar').setHeader('testheader3', 'xyz');
  // Test that chaining works for setHeader.
  assert.deepStrictEqual(res.getHeaders(), expected);
  res.end('ok');
}));
server.listen(0, () => {
  http.get({ port: server.address().port }, common.mustCall((res) => {
    res.on('data', () => {});
    res.on('end', common.mustCall(() => {
      server.close();
    }));
  }));
});
