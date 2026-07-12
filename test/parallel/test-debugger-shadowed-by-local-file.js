'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');

// When `node inspect` starts the debugger and a file named after the command
// (e.g. `inspect.js`) exists in the current directory, a warning is emitted so
// the shadowing is not silent.
// Refs: https://github.com/nodejs/node/issues/64111
tmpdir.refresh();
fs.writeFileSync(tmpdir.resolve('inspect.js'), '// not run\n');

const script = fixtures.path('debugger', 'three-lines.js');
const cli = startCLI([script], [], { cwd: tmpdir.path });

(async function() {
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    assert.match(
      cli.output,
      /started the debugger; the file 'inspect\.js' in the current directory was not run/,
    );
  } finally {
    const code = await cli.quit();
    assert.strictEqual(code, 0);
  }
})().then(common.mustCall());
