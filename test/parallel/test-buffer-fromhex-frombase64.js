'use strict';
require('../common');
const assert = require('assert');
const { Buffer } = require('buffer');

assert.deepStrictEqual(Buffer.fromHex('f00dcafe'), Buffer.from('f00dcafe', 'hex'));
assert.deepStrictEqual(Buffer.fromHex('F00DCAFE'), Buffer.from('f00dcafe', 'hex'));
assert.deepStrictEqual(Buffer.fromHex(''), Buffer.from('', 'hex'));

assert.throws(() => Buffer.fromHex('0x'), { name: 'SyntaxError' });
assert.throws(() => Buffer.fromHex('a'), { name: 'SyntaxError' });
assert.throws(() => Buffer.fromHex(123), { name: 'TypeError' });
assert.throws(() => Buffer.fromHex('abggcd00'), { name: 'SyntaxError' });

assert.deepStrictEqual(Buffer.fromBase64('SGVsbG8='), Buffer.from('SGVsbG8=', 'base64'));
assert.deepStrictEqual(Buffer.fromBase64('SGV sbG8='), Buffer.from('SGVsbG8=', 'base64'));

assert.deepStrictEqual(
  Buffer.fromBase64('PGJsZXA-PC9ibGVwPg', { alphabet: 'base64url' }),
  Buffer.from('PGJsZXA+PC9ibGVwPg==', 'base64'),
);

assert.deepStrictEqual(Buffer.fromBase64('SGVsbG8=', { lastChunkHandling: 'strict' }), Buffer.from('Hello'));
assert.throws(() => Buffer.fromBase64('SGVsbG8', { lastChunkHandling: 'strict' }), { name: 'SyntaxError' });

assert.deepStrictEqual(
  Buffer.fromBase64('SGVsbG8', { lastChunkHandling: 'stop-before-partial' }),
  Buffer.from('SGVs', 'base64'),
);

assert.throws(() => Buffer.fromBase64('SGV$sbG8=', {}), { name: 'SyntaxError' });
assert.throws(() => Buffer.fromBase64('S', {}), { name: 'SyntaxError' });
assert.throws(() => Buffer.fromBase64(123), { name: 'TypeError' });
assert.throws(() => Buffer.fromBase64('SGVsbG8=', { alphabet: 'unknown' }), { name: 'TypeError' });
