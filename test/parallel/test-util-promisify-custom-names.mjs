import '../common/index.mjs';
import assert from 'node:assert';
import { promisify } from 'node:util';

// Test that customly promisified methods in [util.promisify.custom]
// have appropriate names

import fs from 'node:fs';
import readline from 'node:readline';
import stream from 'node:stream';
import timers from 'node:timers';


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
