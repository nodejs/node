'use strict';

// Context: https://github.com/nodejs/node/issues/45992

require('../common');

const assert = require('assert');
const fs = require('fs');
const readline = require('readline');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
fs.mkdtempSync(`${tmpdir.path}/`);
const path = `${tmpdir.path}/foo`;
const writeStream = fs.createWriteStream(path);

function write(iteration, callback) {
  for (; iteration < 16384; iteration += 1) {
    if (!writeStream.write('foo\r\n')) {
      writeStream.once('drain', () => write(iteration + 1, callback));
      return;
    }
  }

  writeStream.end();
  callback();
}

write(0, () => {
  const input = fs.createReadStream(path);
  const rl = readline.createInterface({ input, crlfDelay: Infinity });
  let carriageReturns = 0;

  rl.on('line', (x) => {
    if (x.includes('\r')) carriageReturns += 1;
  });

  rl.on('close', () => {
    assert.strictEqual(carriageReturns, 0);
  });
});
