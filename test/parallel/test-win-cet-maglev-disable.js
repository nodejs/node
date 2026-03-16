'use strict';
// Verify that the --no-maglev V8 flag is accepted via NODE_OPTIONS on Windows,
// and that the CET shadow stack auto-detection path compiles correctly.
//
// The actual CET auto-disable behavior (in InitializeNodeWithArgsInternal) can
// only be exercised on hardware that has CET shadow stacks enabled by the OS,
// so that path is covered by manual testing on Windows 11 Insider builds.
// This test covers the observable surface: --no-maglev is accepted via
// NODE_OPTIONS without error, and the process exits cleanly.

const common = require('../common');

if (!common.isWindows)
  common.skip('Windows-specific CET / Maglev regression test');

if (process.config.variables.node_without_node_options)
  common.skip('missing NODE_OPTIONS support');

const assert = require('assert');
const { execFile } = require('child_process');

const cases = [
  // --no-maglev must be accepted in NODE_OPTIONS (regression: previously it
  // was a pure V8 passthrough not whitelisted for envvar use).
  { env: 'NODE_OPTIONS=--no-maglev', desc: '--no-maglev via NODE_OPTIONS' },
  // --maglev must also be accepted so users can explicitly opt back in.
  { env: 'NODE_OPTIONS=--maglev',    desc: '--maglev via NODE_OPTIONS' },
];

for (const { env: envStr, desc } of cases) {
  const [key, value] = envStr.split('=');
  const env = { ...process.env, [key]: value };

  execFile(
    process.execPath,
    ['-e', 'process.exitCode = 0;'],
    { env },
    common.mustCall((err, stdout, stderr) => {
      assert.ifError(err);
      assert.strictEqual(stderr, '', `${desc}: unexpected stderr: ${stderr}`);
    })
  );
}
