// This tests that probe mode only activates when --probe is present,
// so that other options can be used as user script arguments without
// accidentally activating probe mode.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const script = fixtures.path('debugger', 'three-lines.js');
const cli = startCLI([
  script,
  '--json',
  '--preview',
  '--timeout=1',
  '--expr',
  'value',
]);

(async () => {
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    await cli.command('exec JSON.stringify(process.argv.slice(2))');
    // Check that it's parsable as usual.
    assert.match(
      cli.output,
      /\["--json","--preview","--timeout=1","--expr","value"\]/,
    );
  } finally {
    const code = await cli.quit();
    assert.strictEqual(code, 0);
  }
})().then(common.mustCall());
