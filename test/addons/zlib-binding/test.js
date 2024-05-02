// Flags: --force-context-aware

'use strict';

const common = require('../../common');
const assert = require('assert');
const zlib = require('zlib');
const binding = require(`./build/${common.buildType}/binding`);

const input = Buffer.from('Hello, World!');

const output = zlib.inflateRawSync(binding.compressBytes(input));
assert.deepStrictEqual(input, output);
