import { mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import { inspect } from 'node:util';

// util.inspect must not be affected by userland mutations of Buffer.prototype
Buffer.prototype.hexSlice = mustNotCall();

const { buffer } = new Uint8Array([0x04, 0x38, 0x73, 0xE6]);
const expected = 'ArrayBuffer { [Uint8Contents]: <04 38 73 e6>, [byteLength]: 4 }';
assert.strictEqual(inspect(buffer), expected);
