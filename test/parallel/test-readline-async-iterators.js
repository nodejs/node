'use strict';

const common = require('../common');
const fs = require('fs');
const { join } = require('path');
const readline = require('readline');
const { Readable } = require('stream');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filename = join(tmpdir.path, 'test.txt');

const testContents = [
  '',
  '\n',
  'line 1',
  'line 1\nline 2 南越国是前203年至前111年存在于岭南地区的一个国家\nline 3\ntrailing',
  'line 1\nline 2\nline 3 ends with newline\n',
];

async function testSimple() {
  for (const fileContent of testContents) {
    fs.writeFileSync(filename, fileContent);

    const readable = fs.createReadStream(filename);
    const rli = readline.createInterface({
      input: readable,
      crlfDelay: Infinity
    });

    const iteratedLines = [];
    for await (const k of rli) {
      iteratedLines.push(k);
    }

    const expectedLines = fileContent.split('\n');
    if (expectedLines[expectedLines.length - 1] === '') {
      expectedLines.pop();
    }
    assert.deepStrictEqual(iteratedLines, expectedLines);
    assert.strictEqual(iteratedLines.join(''), fileContent.replace(/\n/g, ''));
  }
}

async function testMutual() {
  for (const fileContent of testContents) {
    fs.writeFileSync(filename, fileContent);

    const readable = fs.createReadStream(filename);
    const rli = readline.createInterface({
      input: readable,
      crlfDelay: Infinity
    });

    const expectedLines = fileContent.split('\n');
    if (expectedLines[expectedLines.length - 1] === '') {
      expectedLines.pop();
    }
    const iteratedLines = [];
    let iterated = false;
    for await (const k of rli) {
      // This outer loop should only iterate once.
      assert.strictEqual(iterated, false);
      iterated = true;
      iteratedLines.push(k);
      for await (const l of rli) {
        iteratedLines.push(l);
      }
      assert.deepStrictEqual(iteratedLines, expectedLines);
    }
    assert.deepStrictEqual(iteratedLines, expectedLines);
  }
}

async function testPerformance() {
  const loremIpsum = `Lorem ipsum dolor sit amet, consectetur adipiscing elit,
sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
Dui accumsan sit amet nulla facilisi morbi tempus iaculis urna.
Eget dolor morbi non arcu risus quis varius quam quisque.
Lacus viverra vitae congue eu consequat ac felis donec.
Amet porttitor eget dolor morbi non arcu.
Velit ut tortor pretium viverra suspendisse.
Mauris nunc congue nisi vitae suscipit tellus.
Amet nisl suscipit adipiscing bibendum est ultricies integer.
Sit amet dictum sit amet justo donec enim diam.
Condimentum mattis pellentesque id nibh tortor id aliquet lectus proin.
Diam in arcu cursus euismod quis viverra nibh.
`;

  const REPETITIONS = 10000;
  const SAMPLE = 100;
  const THRESHOLD = 81;

  function getLoremIpsumStream() {
    const readable = Readable({
      objectMode: true,
    });
    let i = 0;
    readable._read = () => readable.push(
      i++ >= REPETITIONS ? null : loremIpsum
    );
    return readable;
  }

  function oldWay() {
    const readable = new Readable({
      objectMode: true,
      read: () => {
        this.resume();
      },
      destroy: (err, cb) => {
        this.off('line', lineListener);
        this.off('close', closeListener);
        this.close();
        cb(err);
      },
    });
    const lineListener = (input) => {
      if (!readable.push(input)) {
        // TODO(rexagod): drain to resume flow
        this.pause();
      }
    };
    const closeListener = () => {
      readable.push(null);
    };
    const errorListener = (err) => {
      readable.destroy(err);
    };
    this.on('error', errorListener);
    this.on('line', lineListener);
    this.on('close', closeListener);
    return readable[Symbol.asyncIterator]();
  }

  function getAvg(mean, x, n) {
    return (mean * n + x) / (n + 1);
  }

  let totalTimeOldWay = 0;
  let totalTimeNewWay = 0;
  let totalCharsOldWay = 0;
  let totalCharsNewWay = 0;
  const linesOldWay = [];
  const linesNewWay = [];

  for (let time = 0; time < SAMPLE; time++) {
    const rlOldWay = readline.createInterface({
      input: getLoremIpsumStream(),
    });
    let currenttotalTimeOldWay = Date.now();
    for await (const line of oldWay.call(rlOldWay)) {
      totalCharsOldWay += line.length;
      if (time === 0) {
        linesOldWay.push(line);
      }
    }
    currenttotalTimeOldWay = Date.now() - currenttotalTimeOldWay;
    totalTimeOldWay = getAvg(totalTimeOldWay, currenttotalTimeOldWay, SAMPLE);

    const rlNewWay = readline.createInterface({
      input: getLoremIpsumStream(),
    });
    let currentTotalTimeNewWay = Date.now();
    for await (const line of rlNewWay) {
      totalCharsNewWay += line.length;
      if (time === 0) {
        linesNewWay.push(line);
      }
    }
    currentTotalTimeNewWay = Date.now() - currentTotalTimeNewWay;
    totalTimeNewWay = getAvg(totalTimeNewWay, currentTotalTimeNewWay, SAMPLE);
  }

  assert.strictEqual(totalCharsOldWay, totalCharsNewWay);
  assert.strictEqual(linesOldWay.length, linesNewWay.length);
  linesOldWay.forEach((line, index) =>
    assert.strictEqual(line, linesNewWay[index])
  );
  const percentage = totalTimeNewWay * 100 / totalTimeOldWay;
  assert.ok(percentage <= THRESHOLD, `Failed: ${totalTimeNewWay} isn't lesser than ${THRESHOLD}% of ${totalTimeOldWay}. Actual percentage: ${percentage.toFixed(2)}%`);
}

async function testSlowStreamForLeaks() {
  const message = 'a\nb\nc\n';
  const DELAY = 1;
  const REPETITIONS = 100;
  const warningCallback = common.mustNotCall();
  process.on('warning', warningCallback);

  function getStream() {
    const readable = Readable({
      objectMode: true,
    });
    readable._read = () => {};
    let i = REPETITIONS;
    function schedule() {
      setTimeout(() => {
        i--;
        if (i < 0) {
          readable.push(null);
        } else {
          readable.push(message);
          schedule();
        }
      }, DELAY);
    }
    schedule();
    return readable;
  }
  const iterable = readline.createInterface({
    input: getStream(),
  });

  let lines = 0;
  // eslint-disable-next-line no-unused-vars
  for await (const _ of iterable) {
    lines++;
  }

  assert.strictEqual(lines, 3 * REPETITIONS);
  process.off('warning', warningCallback);
}

testSimple()
  .then(testMutual)
  .then(testPerformance)
  .then(testSlowStreamForLeaks)
  .then(common.mustCall());
