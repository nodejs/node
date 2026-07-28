'use strict';

const common = require('../../common');
const assert = require('assert');
const { parseEncoding } = require(`./build/${common.buildType}/binding`);


assert.strictEqual(parseEncoding('ascii'), 'ASCII');
assert.strictEqual(parseEncoding('ASCII'), 'ASCII');
assert.strictEqual(parseEncoding('base64'), 'BASE64');
assert.strictEqual(parseEncoding('BASE64'), 'BASE64');
assert.strictEqual(parseEncoding('base64url'), 'BASE64URL');
assert.strictEqual(parseEncoding('BASE64URL'), 'BASE64URL');
assert.strictEqual(parseEncoding('binary'), 'LATIN1');
assert.strictEqual(parseEncoding('BINARY'), 'LATIN1');
assert.strictEqual(parseEncoding('buffer'), 'BUFFER');
assert.strictEqual(parseEncoding('BUFFER'), 'BUFFER');
assert.strictEqual(parseEncoding('hex'), 'HEX');
assert.strictEqual(parseEncoding('HEX'), 'HEX');
assert.strictEqual(parseEncoding('latin1'), 'LATIN1');
assert.strictEqual(parseEncoding('LATIN1'), 'LATIN1');

// ucs2 variations
assert.strictEqual(parseEncoding('ucs2'), 'UCS2');
assert.strictEqual(parseEncoding('ucs-2'), 'UCS2');
assert.strictEqual(parseEncoding('UCS2'), 'UCS2');
assert.strictEqual(parseEncoding('UCS-2'), 'UCS2');

// utf8 variations
assert.strictEqual(parseEncoding('utf8'), 'UTF8');
assert.strictEqual(parseEncoding('utf-8'), 'UTF8');
assert.strictEqual(parseEncoding('UTF8'), 'UTF8');
assert.strictEqual(parseEncoding('UTF-8'), 'UTF8');

// utf16le variations
assert.strictEqual(parseEncoding('utf16le'), 'UCS2');
assert.strictEqual(parseEncoding('utf-16le'), 'UCS2');
assert.strictEqual(parseEncoding('UTF16LE'), 'UCS2');
assert.strictEqual(parseEncoding('UTF-16LE'), 'UCS2');

// unknown cases
assert.strictEqual(parseEncoding(''), 'UNKNOWN');
assert.strictEqual(parseEncoding('asCOO'), 'UNKNOWN');
assert.strictEqual(parseEncoding('hux'), 'UNKNOWN');
assert.strictEqual(parseEncoding('utf-buffer'), 'UNKNOWN');
assert.strictEqual(parseEncoding('utf-16leNOT'), 'UNKNOWN');
assert.strictEqual(parseEncoding('linary'), 'UNKNOWN');
assert.strictEqual(parseEncoding('luffer'), 'UNKNOWN');
