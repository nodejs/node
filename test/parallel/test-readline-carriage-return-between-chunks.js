'use strict';

const common = require('../common');

const assert = require('node:assert');
const readline = require('node:readline');
const { Readable } = require('node:stream');


const input = Readable.from((function*() {
  yield 'a\nb';
  yield '\r\n';
})());
const rl = readline.createInterface({ input, crlfDelay: Infinity });
let carriageReturns = 0;

rl.on('line', (line) => {
  if (line.includes('\r')) carriageReturns++;
});

rl.on('close', common.mustCall(() => {
  assert.strictEqual(carriageReturns, 0);
}));
