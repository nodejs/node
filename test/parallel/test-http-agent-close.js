'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const agent = new http.Agent();
const _err = new Error('kaboom');
agent.createSocket = function(req, options, cb) {
  cb(_err);
};

const req = http
  .request({
    agent
  })
  .on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
  }))
  .on('close', common.mustCall(() => {
    assert.strictEqual(req.destroyed, true);
  }));
