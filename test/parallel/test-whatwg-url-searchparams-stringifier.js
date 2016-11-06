'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const m = new URL('http://example.org');
let params = m.searchParams;

// Until we export URLSearchParams
const URLSearchParams = params.constructor;

// Serialize space
// querystring does not currently handle spaces intelligently
// params = new URLSearchParams();
// params.append('a', 'b c');
// assert.strictEqual(params + '', 'a=b+c');
// params.delete('a');
// params.append('a b', 'c');
// assert.strictEqual(params + '', 'a+b=c');

// Serialize empty value
params = new URLSearchParams();
params.append('a', '');
assert.strictEqual(params + '', 'a=');
params.append('a', '');
assert.strictEqual(params + '', 'a=&a=');
params.append('', 'b');
assert.strictEqual(params + '', 'a=&a=&=b');
params.append('', '');
assert.strictEqual(params + '', 'a=&a=&=b&=');
params.append('', '');
assert.strictEqual(params + '', 'a=&a=&=b&=&=');

// Serialize empty name
params = new URLSearchParams();
params.append('', 'b');
assert.strictEqual(params + '', '=b');
params.append('', 'b');
assert.strictEqual(params + '', '=b&=b');

// Serialize empty name and value
params = new URLSearchParams();
params.append('', '');
assert.strictEqual(params + '', '=');
params.append('', '');
assert.strictEqual(params + '', '=&=');

// Serialize +
params = new URLSearchParams();
params.append('a', 'b+c');
assert.strictEqual(params + '', 'a=b%2Bc');
params.delete('a');
params.append('a+b', 'c');
assert.strictEqual(params + '', 'a%2Bb=c');

// Serialize =
params = new URLSearchParams();
params.append('=', 'a');
assert.strictEqual(params + '', '%3D=a');
params.append('b', '=');
assert.strictEqual(params + '', '%3D=a&b=%3D');

// Serialize &
params = new URLSearchParams();
params.append('&', 'a');
assert.strictEqual(params + '', '%26=a');
params.append('b', '&');
assert.strictEqual(params + '', '%26=a&b=%26');

// Serialize *-._
params = new URLSearchParams();
params.append('a', '*-._');
assert.strictEqual(params + '', 'a=*-._');
params.delete('a');
params.append('*-._', 'c');
assert.strictEqual(params + '', '*-._=c');

// Serialize %
params = new URLSearchParams();
params.append('a', 'b%c');
assert.strictEqual(params + '', 'a=b%25c');
params.delete('a');
params.append('a%b', 'c');
assert.strictEqual(params + '', 'a%25b=c');

// Serialize \\0
params = new URLSearchParams();
params.append('a', 'b\0c');
assert.strictEqual(params + '', 'a=b%00c');
params.delete('a');
params.append('a\0b', 'c');
assert.strictEqual(params + '', 'a%00b=c');

// Serialize \uD83D\uDCA9
// Unicode Character 'PILE OF POO' (U+1F4A9)
params = new URLSearchParams();
params.append('a', 'b\uD83D\uDCA9c');
assert.strictEqual(params + '', 'a=b%F0%9F%92%A9c');
params.delete('a');
params.append('a\uD83D\uDCA9b', 'c');
assert.strictEqual(params + '', 'a%F0%9F%92%A9b=c');

// URLSearchParams.toString

// querystring parses `&&` as {'': ''}
// params = new URLSearchParams('a=b&c=d&&e&&');
// assert.strictEqual(params.toString(), 'a=b&c=d&e=');

// querystring does not currently handle spaces intelligently
// params = new URLSearchParams('a = b &a=b&c=d%20');
// assert.strictEqual(params.toString(), 'a+=+b+&a=b&c=d+');

// The lone '=' _does_ survive the roundtrip.
params = new URLSearchParams('a=&a=b');
assert.strictEqual(params.toString(), 'a=&a=b');
