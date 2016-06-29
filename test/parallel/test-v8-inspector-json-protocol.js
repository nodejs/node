// Flags: --inspect={PORT}
'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const options = {
  path: '/json/protocol',
  port: common.PORT,
  host: common.localhostIPv4,
};

http.get(options, common.mustCall((res) => {
  let body = '';
  res.setEncoding('utf8');
  res.on('data', (data) => body += data);
  res.on('end', common.mustCall(() => {
    assert(body.length > 0);
    assert.deepStrictEqual(JSON.stringify(JSON.parse(body)), body);
  }));
}));
