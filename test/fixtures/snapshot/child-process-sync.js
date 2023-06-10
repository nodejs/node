'use strict';

const {
  setDeserializeMainFunction,
  isBuildingSnapshot
} = require('v8').startupSnapshot;
const escapePOSIXShell = require('../../common/escapePOSIXShell');

function spawn() {
  const { spawnSync, execFileSync, execSync } = require('child_process');
  spawnSync(process.execPath, [ __filename, 'spawnSync' ], { stdio: 'inherit' });
  const [cmd, opts] = escapePOSIXShell`"${process.execPath}" "${__filename}" "execSync"`;
  execSync(cmd, { ...opts, stdio: 'inherit'});
  execFileSync(process.execPath, [ __filename, 'execFileSync' ], { stdio: 'inherit' });
}

if (process.argv[2] !== undefined) {
  console.log('From child process', process.argv[2]);
} else {
  spawn();
}

if (isBuildingSnapshot()) {
  setDeserializeMainFunction(() => {
    spawn();
  });
}
