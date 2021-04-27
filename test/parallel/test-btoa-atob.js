'use strict';

require('../common');
const assert = require('assert');

let s1 = '';
for (let i = 0; i < 256; ++i) s1 += String.fromCharCode(i);
const s1B64 = Buffer.from(s1).toString('base64');

assert.strictEqual(btoa(s1), s1B64);

const s2 = 'hello world';
const s2B64 = Buffer.from(s2).toString('base64');
assert.strictEqual(btoa(s2), s2B64);

const s3 = 'BlingBling...';
const s3B64 = Buffer.from(s3).toString('base64');
assert.strictEqual(btoa(s3), s3B64);

const s4 = '哇咔咔';
const s4B64 = Buffer.from(s4).toString('base64');
assert.throws(() => { btoa(s4); }, {
  name: 'InvalidCharacterError',
  message: 'Invalid character',
  code: 5,
});

assert.strictEqual(atob(s1B64),
                   Buffer.from(s1B64, 'base64').toString('latin1'));
assert.strictEqual(atob(s2B64),
                   Buffer.from(s2B64, 'base64').toString('latin1'));
assert.strictEqual(atob(s3B64),
                   Buffer.from(s3B64, 'base64').toString('latin1'));
assert.strictEqual(atob(s4B64),
                   Buffer.from(s4B64, 'base64').toString('latin1'));
