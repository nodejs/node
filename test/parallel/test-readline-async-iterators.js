'use strict';

const common = require('../common');
const fs = require('fs');
const { join } = require('path');
const readline = require('readline');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filename = join(tmpdir.path, 'test.txt');

const testContents = [
  '',
  '\n',
  'line 1',
  'line 1\nline 2 南越国是前203年至前111年存在于岭南地区的一个国家\nline 3\ntrailing',
  'line 1\nline 2\nline 3 ends with newline\n'
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
    assert.strictEqual(iteratedLines.join(''), fileContent.replace(/\n/gm, ''));
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

testSimple().then(testMutual).then(common.mustCall());
