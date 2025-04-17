import '../common/index.mjs';
import assert from 'node:assert';
import { promisify } from 'node:util';

// Test that customly promisified methods in [util.promisify.custom]
// have appropriate names

import fs from 'node:fs';
import readline from 'node:readline';
import stream from 'node:stream';
import timers from 'node:timers';
import http2 from 'node:http2';


assert.strictEqual(
  promisify(fs.exists).name,
  'exists'
);

assert.strictEqual(
  promisify(readline.Interface.prototype.question).name,
  'question',
);

assert.strictEqual(
  promisify(stream.finished).name,
  'finished'
);
assert.strictEqual(
  promisify(stream.pipeline).name,
  'pipeline'
);

assert.strictEqual(
  promisify(timers.setImmediate).name,
  'setImmediate'
);
assert.strictEqual(
  promisify(timers.setTimeout).name,
  'setTimeout'
);

assert.strictEqual(
  promisify(http2.connect).name,
  'connect'
);
