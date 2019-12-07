'use strict';

const common = require('../common');
const fs = require('fs');
const { once } = require('events');
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

async function testSimpleDestroy() {
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
      break;
    }

    const expectedLines = fileContent.split('\n');
    if (expectedLines[expectedLines.length - 1] === '') {
      expectedLines.pop();
    }
    expectedLines.splice(1);

    assert.deepStrictEqual(iteratedLines, expectedLines);

    rli.close();
    readable.destroy();

    await once(readable, 'close');
  }
}

async function testMutualDestroy() {
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
    expectedLines.splice(2);

    const iteratedLines = [];
    for await (const k of rli) {
      iteratedLines.push(k);
      for await (const l of rli) {
        iteratedLines.push(l);
        break;
      }
      assert.deepStrictEqual(iteratedLines, expectedLines);
    }

    assert.deepStrictEqual(iteratedLines, expectedLines);

    rli.close();
    readable.destroy();

    await once(readable, 'close');
  }
}

testSimpleDestroy().then(testMutualDestroy).then(common.mustCall());
