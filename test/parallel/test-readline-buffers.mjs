import '../common/index.mjs';

import { createInterface } from 'readline';
import { Readable } from 'stream';
import { setImmediate } from 'timers/promises';
import { deepStrictEqual } from 'assert';

const stream = Readable.from(async function* () {
  for (let i = 0; i < 50; i++) {
    yield new Buffer.from(i + '\r\n', 'utf-8');
    await Promise.resolve();
  }
}(), { objectMode: false });

// Promises don't keep the event loop alive so to avoid the process exiting
// below we create an interval;
const interval = setInterval(() => { }, 1000);

const rl = createInterface({
  input: stream,
  crlfDelay: Infinity
});

await setImmediate();
const result = [];
for await (const item of rl) {
  result.push(item);
}
deepStrictEqual(result, Object.keys(Array(50).fill()));
clearInterval(interval);
