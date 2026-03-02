'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const debug = require('node:util').debuglog('test');

const testResBody = 'response content\n';

{
  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    assert.throws(() => {
      res.writeEarlyHints('bad argument type');
    }, (err) => err.code === 'ERR_INVALID_ARG_TYPE');

    assert.throws(() => {
      res.writeEarlyHints({
        link: '</>; '
      });
    }, (err) => err.code === 'ERR_INVALID_ARG_VALUE');

    assert.throws(() => {
      res.writeEarlyHints({
        link: 'rel=preload; </scripts.js>'
      });
    }, (err) => err.code === 'ERR_INVALID_ARG_VALUE');

    assert.throws(() => {
      res.writeEarlyHints({
        link: 'invalid string'
      });
    }, (err) => err.code === 'ERR_INVALID_ARG_VALUE');

    assert.throws(() => {
      res.writeEarlyHints({
        'link': '</styles.css>; rel=preload; as=style',
        'x-bad\r\n': 'value',
      });
    }, (err) => err.code === 'ERR_INVALID_HTTP_TOKEN');

    assert.throws(() => {
      res.writeEarlyHints({
        'link': '</styles.css>; rel=preload; as=style',
        'x-custom': 'bad\r\nvalue',
      });
    }, (err) => err.code === 'ERR_INVALID_CHAR');

    debug('Server sending full response...');
    res.end(testResBody);
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });

    req.end();
    debug('Client sending request...');

    req.on('information', common.mustNotCall());
  }));
}
