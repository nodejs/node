import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { spawnSyncAndExitWithoutError } from '../common/child_process.js';

async function runTests(run, reporters) {
  for (const reporterName of Object.keys(reporters)) {
    if (reporterName === 'default') continue;
    console.log({ reporterName });

    const stream = run({
      files: undefined
    }).compose(reporters[reporterName]);
    stream.on('test:fail', () => {
      throw new Error('Received test:fail with ' + reporterName);
    });
    stream.on('test:pass', () => {
      throw new Error('Received test:pass with ' + reporterName);
    });

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  }
}

tmpdir.refresh();
spawnSyncAndExitWithoutError(process.execPath, ['--input-type=module', '-e', `
  import { run } from 'node:test';
  import * as reporters from 'node:test/reporters';

  await (${runTests})(run, reporters);
`], { cwd: tmpdir.path });
