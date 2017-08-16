'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http = require('http');
const https = require('https');
const error = 'Unable to determine the domain name';

function test(host) {
  ['get', 'request'].forEach((method) => {
    [http, https].forEach((module) => {
      assert.throws(() => module[method](host, () => {
        throw new Error(`${module}.${method} should not connect to ${host}`);
      }), error);
    });
  });
}

['www.nodejs.org', 'localhost', '127.0.0.1', 'http://:80/'].forEach(test);
