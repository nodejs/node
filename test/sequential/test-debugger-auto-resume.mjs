import { skipIfInspectorDisabled } from '../common/index.mjs';

skipIfInspectorDisabled();

import { path as _path } from '../common/fixtures.js';
import startCLI from '../common/debugger.js';
import { addLibraryPath } from '../common/shared-lib-util.js';

import { deepStrictEqual, strictEqual } from 'assert';
import { relative } from 'path';

addLibraryPath(process.env);

// Auto-resume on start if the environment variable is defined.
{
  const scriptFullPath = _path('debugger', 'break.js');
  const script = relative(process.cwd(), scriptFullPath);

  const env = {
    ...process.env,
  };
  env.NODE_INSPECT_RESUME_ON_START = '1';

  const cli = startCLI(['--port=0', script], [], {
    env,
  });

  await cli.waitForInitialBreak();
  deepStrictEqual(cli.breakInfo, {
    filename: script,
    line: 10,
  });
  const code = await cli.quit();
  strictEqual(code, 0);
}
