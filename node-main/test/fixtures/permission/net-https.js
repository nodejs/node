'use strict';

const common = require('../../common');
const assert = require('assert');
const https = require('https');

const url = process.env.URL;

{
  const options = {
    rejectUnauthorized: false
  };

  const req = https.get(url, options, common.mustNotCall('HTTPS request should be blocked'));

  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}