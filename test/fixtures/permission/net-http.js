'use strict';

const common = require('../../common');
const assert = require('assert');
const http = require('http');

const url = process.env.URL;

{
  const req = http.get(url, common.mustNotCall('HTTP request should be blocked'));

  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}
