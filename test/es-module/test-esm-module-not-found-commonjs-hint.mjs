import '../common/index.mjs';
import { fixturesDir } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

[
  {
    input: 'import "./print-error-message"',
    expected: 'Did you mean to import ../print-error-message.js?'
  },
  {
    input: 'import obj from "some_module/obj"',
    expected: 'Did you mean to import some_module/obj.js?'
  },
].forEach(({ input, expected }) => {
  const child = spawn(execPath, [
    '--input-type=module',
    '--eval',
    input,
  ], {
    cwd: fixturesDir
  });

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', () => {
    ok(stderr.toString().includes(expected));
  });
});
