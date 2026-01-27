'use strict';

require('../common');

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

// This test verifies that fs.globSync() works correctly udner permission model
// when --allow-fs-read is granted only to the certain directory

// Example:
// Directory structure:
//   somedir/
//     file1.js
//   sample.js
//
// Command:
//   node --permission --allow-fs-read=somedir/ sample.js
//
// Code:
//   fs.globSync('somedir/*') <- sould find file1.js

tmpdir.refresh();

const someDir = tmpdir.resolve('somedir');
const script = tmpdir.resolve('sample.js');

fs.mkdirSync(someDir);
fs.writeFileSync(
  path.join(someDir, 'file1.js'),
  'console.log("test");'
);

fs.writeFileSync(
  script,
  `
'use strict';
const fs = require('fs');
const matches = fs.globSync('somedir/*');
console.log(JSON.stringify(matches));
`
);

spawnSyncAndAssert(
  process.execPath,
  [
    '--permission',
    `--allow-fs-read=${someDir}`,
    script,
  ],
  {
    cwd: tmpdir.path,
  },
  {
    stdout(output) {
      assert.deepStrictEqual(
        JSON.parse(output),
        ['somedir/file1.js']
      );
      return true;
    },
    stderr(output) {
      assert.doesNotMatch(output, /ERR_ACCESS_DENIED/);
      return true;
    },
  }
);
