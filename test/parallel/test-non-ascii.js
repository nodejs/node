'use strict';

const fs = require('node:fs');
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { describe, it } = require('node:test');
const assert = require('node:assert');

test('Running from a directory with non-ASCII characters', async () => {
    fs.mkdirSync(tmpdir.resolve('12月'), { recursive: true });
    fs.writeFileSync(
      tmpdir.resolve('12月/index.js'),
      "console.log('12月');",
    );
    await common.spawnPromisified(process.execPath, [tmpdir.resolve('12月/index.js')]).then(common.mustCall((result) => {
       assert.strictEqual(result.stdout, '12月' + '\n');
     }));
  });
});