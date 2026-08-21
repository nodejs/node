'use strict';

const common = require('../common');
const fs = require('fs');
const readline = require('readline');
const { Readable } = require('stream');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filename = tmpdir.resolve('test.txt');

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
  .then(testSlowStreamForLeaks)
  .then(common.mustCall());
