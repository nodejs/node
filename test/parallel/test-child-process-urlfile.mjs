import { isWindows, mustCall, mustSucceed, mustNotCall } from '../common/index.mjs';
import { strictEqual } from 'node:assert';
import { pathToFileURL } from 'node:url';
import cp from 'node:child_process';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const cwd = tmpdir.path;
const whichCommand = isWindows ? 'where.exe cmd.exe' : 'which pwd';
const pwdFullPath = `${cp.execSync(whichCommand)}`.trim();
const pwdUrl = pathToFileURL(pwdFullPath);
const pwdCommandAndOptions = isWindows ?
  [pwdUrl, ['/d', '/c', 'cd'], { cwd: pathToFileURL(cwd) }] :
  [pwdUrl, [], { cwd: pathToFileURL(cwd) }];

{
  cp.execFile(...pwdCommandAndOptions, mustSucceed((stdout) => {
    strictEqual(`${stdout}`.trim(), cwd);
  }));
}

{
  const stdout = cp.execFileSync(...pwdCommandAndOptions);
  strictEqual(`${stdout}`.trim(), cwd);
}

{
  const pwd = cp.spawn(...pwdCommandAndOptions);
  pwd.stdout.on('data', mustCall((stdout) => {
    strictEqual(`${stdout}`.trim(), cwd);
  }));
  pwd.stderr.on('data', mustNotCall());
  pwd.on('close', mustCall((code) => {
    strictEqual(code, 0);
  }));
}

{
  const stdout = cp.spawnSync(...pwdCommandAndOptions).stdout;
  strictEqual(`${stdout}`.trim(), cwd);
}
