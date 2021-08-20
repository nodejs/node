'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const { addLibraryPath } = require('../common/shared-lib-util');

const assert = require('assert');
const path = require('path');

addLibraryPath(process.env);

// Auto-resume on start if the environment variable is defined.
{
  const scriptFullPath = fixtures.path('debugger', 'break.js');
  const script = path.relative(process.cwd(), scriptFullPath);

  const env = { ...process.env };
  env.NODE_INSPECT_RESUME_ON_START = '1';

  const cli = startCLI([script], [], { env });

  cli.waitForInitialBreak()
    .then(() => {
      assert.deepStrictEqual(
        cli.breakInfo,
        { filename: script, line: 10 },
      );
    })
    .then(() => cli.quit())
    .then((code) => {
      assert.strictEqual(code, 0);
    });
}
