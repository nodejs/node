'use strict';

const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const path = require('path');

const fixture = path.join(common.fixturesDir, 'exit.js');

{
  execFile(
    process.execPath,
    [fixture, 42],
    common.mustCall((e) => {
      // Check that arguments are included in message
      assert.strictEqual(e.message.trim(),
                         `Command failed: ${process.execPath} ${fixture} 42`);
      assert.strictEqual(e.code, 42);
    })
  );
}
