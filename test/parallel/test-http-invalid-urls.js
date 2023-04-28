/* eslint-disable node-core/crypto-check */

'use strict';

const common = require('../common');

const assert = require('assert');
const http = require('http');
const modules = { 'http': http };

if (common.hasCrypto) {
  const https = require('https');
  modules.https = https;
}

function test(host) {
  ['get', 'request'].forEach((fn) => {
    Object.keys(modules).forEach((module) => {
      const doNotCall = common.mustNotCall(
        `${module}.${fn} should not connect to ${host}`
      );
      const throws = () => { modules[module][fn](host, doNotCall); };
      assert.throws(throws, {
        name: 'TypeError',
        code: 'ERR_INVALID_URL'
      });
    });
  });
}

['www.nodejs.org', 'localhost', '127.0.0.1', 'http://:80/'].forEach(test);
