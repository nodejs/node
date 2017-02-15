'use strict';

const common = require('../../common');
const assert = require('assert');
const { parseEncoding } = require(`./build/${common.buildType}/binding`);

assert.strictEqual(parseEncoding(''), 'UNKNOWN');

assert.strictEqual(parseEncoding('ascii'), 'ASCII');
assert.strictEqual(parseEncoding('base64'), 'BASE64');
assert.strictEqual(parseEncoding('binary'), 'LATIN1');
assert.strictEqual(parseEncoding('buffer'), 'BUFFER');
assert.strictEqual(parseEncoding('hex'), 'HEX');
assert.strictEqual(parseEncoding('latin1'), 'LATIN1');
assert.strictEqual(parseEncoding('ucs2'), 'UCS2');
assert.strictEqual(parseEncoding('utf8'), 'UTF8');

assert.strictEqual(parseEncoding('linary'), 'UNKNOWN');
assert.strictEqual(parseEncoding('luffer'), 'UNKNOWN');
