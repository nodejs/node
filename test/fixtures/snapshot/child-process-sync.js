'use strict';

const {
  setDeserializeMainFunction,
  isBuildingSnapshot
} = require('v8').startupSnapshot;

/**
 * @see {import('../../common').escapePOSIXShell}
 */
function escapePOSIXShell(cmdParts, ...args) {
  if (process.platform === 'win32') {
    // On Windows, paths cannot contain `"`, so we can return the string unchanged.
    return [String.raw({ raw: cmdParts }, ...args)];
  }
  // On POSIX shells, we can pass values via the env, as there's a standard way for referencing a variable.
  const env = { ...process.env };
  let cmd = cmdParts[0];
  for (let i = 0; i < args.length; i++) {
    const envVarName = `ESCAPED_${i}`;
    env[envVarName] = args[i];
    cmd += '${' + envVarName + '}' + cmdParts[i + 1];
  }

  return [cmd, { env }];
}

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
