import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import * as fixtures from '../common/fixtures.mjs';
import startCLI from '../common/debugger.js';
import { addLibraryPath } from '../common/shared-lib-util.js';

import assert from 'assert';
import { relative } from 'path';

addLibraryPath(process.env);

// Auto-resume on start if the environment variable is defined.
{
  const scriptFullPath = fixtures.path('debugger', 'break.js');
  const script = relative(process.cwd(), scriptFullPath);

  const env = {
    ...process.env,
  };
  env.NODE_INSPECT_RESUME_ON_START = '1';

  const cli = startCLI([script], [], { env });

  await cli.waitForInitialBreak();
  assert.deepStrictEqual(cli.breakInfo, {
    filename: script,
    line: 10,
  });
  const code = await cli.quit();
  assert.strictEqual(code, 0);
}
