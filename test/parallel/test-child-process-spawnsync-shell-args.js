'use strict';
const common = require('../common');
const { ok: mkay, strictEqual: same } = require('assert');
const cp = require('child_process');

// This test ensures that child_process.spawnSync escapes shell args as
// required since #29532.
const run = (shell, command, args, ok = mkay) => {
  // eslint-disable-next-line prefer-const
  let { stdout, stderr, error, status } = cp.spawnSync(command, args, {
    shell: shell,
    shellEscape: true
  });
  [stdout, stderr] = [stdout, stderr].map((i) => i.toString());
  ok(!error && !stderr, [error, status, stderr, stdout].join(' ||| '));
  return common.isWindows ? stdout.replace(/\r/g, '') : stdout;
};

// find bash
const silent = () => {};
let bashPath = '';
if (common.isWindows) {
  const bashPaths = run(true, 'where bash', null, silent)
    .trim()
    .split(/[\n]+/g);
  // We may have multiple results. Pick the one that looks right.
  for (const i of bashPaths) {
    // Not you, Bash on Windows stub.
    // (It parses the cmdline... quite differently.)
    if (i.indexOf('Windows\\System32') < 0) {
      bashPath = i;
      break;
    }
  }
} else {
  bashPath = run(true, 'command -v bash', null, silent).trim();
}

if (!bashPath) common.skip('Skipping test because bash is not found.');
else console.info(`Using bash from ${bashPath}.`);

const reprint = "printf '%q '";
const testArgs = ['  Lore"> Ipsu""<\nd|or\tsit', 'on \\a\x07 ringing ch&air'];
const testArgsLite = ['He\\\\o \\"Wor\tld\\|<>&*\\', "y'all\x07"];
const reprintArgs = ['-c', `${reprint} "$@"`, '--', ...testArgs];
const nodeEcho = [
  '-e',
  'console.log(process.argv.slice(1).join("\\n"))',
  '--',
];

const echoOut = testArgs.concat(['']).join('\n');
same(run(bashPath, 'printf "%s\n"', testArgs), echoOut);
same(run(false, process.execPath, nodeEcho.concat(testArgs)), echoOut);

const echoOutLite = testArgsLite.concat(['']).join('\n');
// Git MSYS Bash cmdline parsing quirk
if (!common.isWindows)
  same(run(bashPath, 'printf "%s\n"', testArgsLite), echoOutLite);

const reprintOut = run(bashPath, reprint, testArgs);
same(run(false, 'bash', reprintArgs), reprintOut);

if (common.isWindows) {
  same(run('powershell', 'Write-Output', testArgs), echoOut);

  // XXX: Here we see where pwsh internal cmdline quoting clashes with actual
  // Windows cmdline handling. Microsoft, don't you have the source for both?
  //
  // Ref: PowerShell/PowerShell#1995
  // same(
  //   run('powershell', `& "${process.execPath}"`, nodeEcho.concat(testArgs)),
  //   echoOut
  // );

  // Cmd has troubles with newlines, so we lower the bar
  // (not using 'echo' because cmd handles builtin parsing differently)
  same(
    run('cmd', `"${process.execPath}"`, nodeEcho.concat(testArgsLite)),
    echoOutLite
  );
}
