/* eslint-disable node-core/crypto-check */
// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const { kOutHeaders } = require('internal/http');

const http = require('http');
const modules = { http };

if (common.hasCrypto) {
  const https = require('https');
  modules.https = https;
}

Object.keys(modules).forEach((module) => {
  const doNotCall = common.mustNotCall(
    `${module}.request should not connect to ${module}://example.com%60x.example.com`
  );
  const req = modules[module].request(`${module}://example.com%60x.example.com`, doNotCall);
  assert.deepStrictEqual(req[kOutHeaders].host, [
    'Host',
    'example.com`x.example.com',
  ]);
  req.abort();
});
