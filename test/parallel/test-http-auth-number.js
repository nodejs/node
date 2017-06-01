'use strict';

require('../common');
const http = require('http');
const url = require('url');
const assert = require('assert');

const opts = url.parse('http://127.0.0.1:8180');
opts.auth = 100;

assert.throws(() => {
  http.get(opts);
}, /^TypeError: "value" argument must not be a number$/);
