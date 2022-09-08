import * as common from '../common/index.mjs';
import * as tmpdir from '../common/tmpdir.js';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { NodeInstance } from '../common/inspector-helper.js';


common.skipIfInspectorDisabled();
tmpdir.refresh();

{
  const child = new NodeInstance(['--test', '--inspect-brk=0'], undefined, fixtures.path('test-runner/index.test.js'));

  let stdout = '';
  let stderr = '';
  child.on('stdout', (line) => stdout += line);
  child.on('stderr', (line) => stderr += line);

  const session = await child.connectInspectorSession();

  await session.send([
    { method: 'Runtime.enable' },
    { method: 'Runtime.runIfWaitingForDebugger' }]);

  session.disconnect();
  assert.match(stderr,
               /Warning: Using the inspector with --test forces running at a concurrency of 1\. Use the inspectPort option to run with concurrency/);
}


{
  const args = ['--test', '--inspect=0', fixtures.path('test-runner/index.js')];
  const { stderr, stdout, code, signal } = await common.spawnPromisified(process.execPath, args);

  assert.match(stderr,
               /Warning: Using the inspector with --test forces running at a concurrency of 1\. Use the inspectPort option to run with concurrency/);
  assert.match(stdout, /not ok 1 - .+index\.js/);
  assert.match(stdout, /stderr: \|-\r?\n\s+Debugger listening on/);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
}


{
  // File not found.
  const args = ['--test', '--inspect=0', 'a-random-file-that-does-not-exist.js'];
  const { stderr, stdout, code, signal } = await common.spawnPromisified(process.execPath, args);

  assert.strictEqual(stdout, '');
  assert.match(stderr, /^Could not find/);
  assert.doesNotMatch(stderr, /Warning: Using the inspector with --test forces running at a concurrency of 1\. Use the inspectPort option to run with concurrency/);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
}
