'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');
const readline = require('readline');

const CONTENT = 'content';
const LINES_PER_PUSH = 2051;
const REPETITIONS = 3;

(async () => {
  const readable = new Readable({ read() {} });
  let salt = 0;
  for (let i = 0; i < REPETITIONS; i++) {
    readable.push(`${CONTENT}\n`.repeat(LINES_PER_PUSH + i));
    salt += i;
  }
  const TOTAL_LINES = LINES_PER_PUSH * REPETITIONS + salt;

  const rli = readline.createInterface({
    input: readable,
    crlfDelay: Infinity
  });

  const it = rli[Symbol.asyncIterator]();
  const watermarkData = it[Symbol.for('nodejs.watermarkData')];
  const highWaterMark = watermarkData.high;

  // For this test to work, we have to queue up more than the number of
  // highWaterMark items in rli. Make sure that is the case.
  assert(TOTAL_LINES > highWaterMark, `TOTAL_LINES (${TOTAL_LINES}) isn't greater than highWaterMark (${highWaterMark})`);

  let iterations = 0;
  let readableEnded = false;
  let notPaused = 0;
  for await (const line of it) {
    assert.strictEqual(readableEnded, false);
    assert.strictEqual(line, CONTENT);
    assert.ok(watermarkData.size <= TOTAL_LINES);
    assert.strictEqual(readable.isPaused(), watermarkData.size >= 1);
    if (!readable.isPaused()) {
      notPaused++;
    }

    iterations += 1;

    // We have to end the input stream asynchronously for back pressure to work.
    // Only end when we have reached the final line.
    if (iterations === TOTAL_LINES) {
      readable.push(null);
      readableEnded = true;
    }
  }

  assert.strictEqual(iterations, TOTAL_LINES);
  assert.strictEqual(notPaused, REPETITIONS);
})().then(common.mustCall());
