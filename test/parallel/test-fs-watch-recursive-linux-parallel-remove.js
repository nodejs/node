'use strict';

const common = require('../common');

if (!common.isLinux)
  common.skip('This test can run only on Linux');

const assert = require('node:assert');
const path = require('node:path');
const fs = require('node:fs');
const { spawn } = require('node:child_process');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

const watcher = fs.watch(testDir, { recursive: true });
watcher.on('change', function(event, filename) {
  // This console.log makes the error happen
  // do not remove
  console.log(filename, event)
  // assert.strictEqual(event, 'rename');
});

const testFile = path.join(testDir, 'a')
const child = spawn(process.argv[0], ['-e', `const fs = require('node:fs'); for (let i = 0; i < 10000; i++) { const fd = fs.openSync('${testFile}', 'w'); fs.writeSync(fd, Buffer.from('hello')); fs.rmSync('${testFile}') }`], {
  stdio: 'inherit'
})

child.on('exit', function () {
  watcher.close()
})
