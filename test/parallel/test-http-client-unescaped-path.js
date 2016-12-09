'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

for (let i = 0; i <= 32; i += 1) {
  const path = 'bad' + String.fromCharCode(i) + 'path';
  assert.throws(() => http.get({ path }, common.fail),
                /contains unescaped characters/);
}
