import { mustCall } from '../common/index.mjs';
import { fixturesDir } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

[
  {
    input: 'import "./print-error-message"',
    // Did you mean to import ../print-error-message.js?
    expected: / \.\.\/print-error-message\.js\?/,
  },
  {
    input: 'import obj from "some_module/obj"',
    expected: / some_module\/obj\.js\?/,
  },
].forEach(({ input, expected }) => {
  const child = spawn(execPath, [
    '--input-type=module',
    '--eval',
    input,
  ], {
    cwd: fixturesDir,
  });

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    notStrictEqual(code, 0);
    match(stderr, expected);
  }));
});
