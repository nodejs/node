'use strict';

const common = require('../common');
const fs = require('fs');
const { join } = require('path');
const readline = require('readline');
const assert = require('assert');

common.crashOnUnhandledRejection();

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filename = join(tmpdir.path, 'test.txt');
const file_content = 'abc123\n' +
                     '南越国是前203年至前111年存在于岭南地区的一个国家\n';
const file_lines_count = 3;
const without_line_breaks = file_content.replace(/(\n)/gm, '');

fs.writeFileSync(filename, file_content);

async function tests() {
  await (async () => {
    const readable = fs.createReadStream(filename);
    const rli = readline.createInterface({
      input: readable,
      crlfDelay: Infinity
    });

    let data = '';
    let iterations = 0;
    for await (const k of rli) {
      data += k;
      iterations += 1;
    }

    assert.strictEqual(data, without_line_breaks);
    assert.strictEqual(iterations, file_lines_count);
  })();
}

// to avoid missing some tests if a promise does not resolve
tests().then(common.mustCall());
