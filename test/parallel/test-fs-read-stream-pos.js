'use strict';

// Refs: https://github.com/nodejs/node/issues/33940

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');
const path = require('path');

tmpdir.refresh();

const file = path.join(tmpdir.path, '/read_stream_pos_test.txt');

fs.writeFileSync(file, '');

let counter = 0;

const writeInterval = setInterval(() => {
  counter = counter + 1;
  const line = `hello at ${counter}\n`;
  fs.writeFileSync(file, line, { flag: 'a' });
}, 1);

const hwm = 10;
let bufs = [];
let isLow = false;
let cur = 0;
let stream;

const readInterval = setInterval(() => {
  if (stream) return;

  stream = fs.createReadStream(file, {
    highWaterMark: hwm,
    start: cur
  });
  stream.on('data', common.mustCallAtLeast((chunk) => {
    cur += chunk.length;
    bufs.push(chunk);
    if (isLow) {
      const brokenLines = Buffer.concat(bufs).toString()
        .split('\n')
        .filter((line) => {
          const s = 'hello at'.slice(0, line.length);
          if (line && !line.startsWith(s)) {
            return true;
          }
          return false;
        });
      assert.strictEqual(brokenLines.length, 0);
      exitTest();
      return;
    }
    if (chunk.length !== hwm) {
      isLow = true;
    }
  }));
  stream.on('end', () => {
    stream = null;
    isLow = false;
    bufs = [];
  });
}, 10);

// Time longer than 90 seconds to exit safely
const endTimer = setTimeout(() => {
  exitTest();
}, 90000);

const exitTest = () => {
  clearInterval(readInterval);
  clearInterval(writeInterval);
  clearTimeout(endTimer);
  if (stream && !stream.destroyed) {
    stream.on('close', () => {
      process.exit();
    });
    stream.destroy();
  } else {
    process.exit();
  }
};
