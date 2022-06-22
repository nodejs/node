/* eslint camelcase: "off" */
const isWindows = require('./is-windows.js')
const setPATH = require('./set-path.js')
const { chmodSync: chmod, unlinkSync: unlink, writeFileSync: writeFile } = require('fs')
const { tmpdir } = require('os')
const { resolve } = require('path')
const which = require('which')
const npm_config_node_gyp = require.resolve('node-gyp/bin/node-gyp.js')
const escape = require('./escape.js')

const makeSpawnArgs = options => {
  const {
    event,
    path,
    scriptShell = isWindows ? process.env.ComSpec || 'cmd' : 'sh',
    env = {},
    stdio,
    cmd,
    args = [],
    stdioString = false,
  } = options

  let scriptFile
  let script = ''
  const isCmd = /(?:^|\\)cmd(?:\.exe)?$/i.test(scriptShell)
  if (isCmd) {
    scriptFile = resolve(tmpdir(), `${event}-${Date.now()}.cmd`)
    script += '@echo off\n'
    script += `${cmd} ${args.map((arg) => escape.cmd(arg)).join(' ')}`
  } else {
    const shellPath = which.sync(scriptShell)
    scriptFile = resolve(tmpdir(), `${event}-${Date.now()}.sh`)
    script += `#!${shellPath}\n`
    script += `${cmd} ${args.map((arg) => escape.sh(arg)).join(' ')}`
  }
  writeFile(scriptFile, script)
  if (!isCmd) {
    chmod(scriptFile, '0775')
  }
  const spawnArgs = isCmd ? ['/d', '/s', '/c', scriptFile] : ['-c', scriptFile]

  const spawnOpts = {
    env: setPATH(path, {
      // we need to at least save the PATH environment var
      ...process.env,
      ...env,
      npm_package_json: resolve(path, 'package.json'),
      npm_lifecycle_event: event,
      npm_lifecycle_script: cmd,
      npm_config_node_gyp,
    }),
    stdioString,
    stdio,
    cwd: path,
    ...(isCmd ? { windowsVerbatimArguments: true } : {}),
  }

  const cleanup = () => {
    // delete the script, this is just a best effort
    try {
      unlink(scriptFile)
    } catch (err) {}
  }

  return [scriptShell, spawnArgs, spawnOpts, cleanup]
}

module.exports = makeSpawnArgs
