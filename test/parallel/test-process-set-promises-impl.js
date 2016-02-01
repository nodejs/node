'use strict';

require('../common');

const fs = require('fs');
var assert = require('assert');

assert.throws(() => {
  process.setPromiseImplementation(null);
});

assert.throws(() => {
  process.setPromiseImplementation('ok');
});

assert.throws(() => {
  process.setPromiseImplementation({});
});

assert.throws(() => {
  process.setPromiseImplementation({
    resolve() {
    }
  });
});

assert.throws(() => {
  process.setPromiseImplementation({
    resolve() {
      return {};
    }
  });
});

process.setPromiseImplementation({
  resolve(promise) {
    return {
      hello: 'world',
      then(fn0, fn1) {
        return promise.then(fn0, fn1);
      }
    };
  }
});

// affects old requires:
const p = fs.readFile(__filename, 'utf8');
p.then((lines) => {
  assert.ok(lines.indexOf("'use strict'") !== -1);
});
assert.equal(p.hello, 'world');

// affects new requires:
const r = require('crypto').randomBytesAsync(16);
r.then((bytes) => {
  assert.ok(Buffer.isBuffer(bytes));
});
assert.equal(r.hello, 'world');
