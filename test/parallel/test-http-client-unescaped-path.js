'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

function* bad() {
  for (let i = 0; i <= 32; i += 1)
    yield 'bad' + String.fromCharCode(i) + 'path';
}

for (const path of bad()) {
  assert.throws(() => http.get({ path }, common.fail),
                /contains unescaped characters/);
}
