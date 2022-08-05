/* eslint camelcase: "off" */
const isWindows = require('./is-windows.js')
const setPATH = require('./set-path.js')
const { unlinkSync: unlink, writeFileSync: writeFile } = require('fs')
const { tmpdir } = require('os')
const { resolve } = require('path')
const which = require('which')
const npm_config_node_gyp = require.resolve('node-gyp/bin/node-gyp.js')
const escape = require('./escape.js')
const { randomBytes } = require('crypto')

const translateWinPathToPosix = (path) => {
  return path
    .replace(/^([A-z]):/, '/$1')
    .replace(/\\/g, '/')
}

const makeSpawnArgs = options => {
  const {
    event,
    path,
    scriptShell = isWindows ? process.env.ComSpec || 'cmd' : 'sh',
    binPaths,
    env = {},
    stdio,
    cmd,
    args = [],
    stdioString = false,
  } = options

  const spawnEnv = setPATH(path, binPaths, {
    // we need to at least save the PATH environment var
    ...process.env,
    ...env,
    npm_package_json: resolve(path, 'package.json'),
    npm_lifecycle_event: event,
    npm_lifecycle_script: cmd,
    npm_config_node_gyp,
  })

  const fileName = escape.filename(`${event}-${randomBytes(4).toString('hex')}`)
  let scriptFile
  let script = ''

  const isCmd = /(?:^|\\)cmd(?:\.exe)?$/i.test(scriptShell)
  if (isCmd) {
    let initialCmd = ''
    let insideQuotes = false
    for (let i = 0; i < cmd.length; ++i) {
      const char = cmd.charAt(i)
      if (char === ' ' && !insideQuotes) {
        break
      }

      initialCmd += char
      if (char === '"' || char === "'") {
        insideQuotes = !insideQuotes
      }
    }

    let pathToInitial
    try {
      pathToInitial = which.sync(initialCmd, {
        path: spawnEnv.path,
        pathext: spawnEnv.pathext,
      }).toLowerCase()
    } catch (err) {
      pathToInitial = initialCmd.toLowerCase()
    }

    const doubleEscape = pathToInitial.endsWith('.cmd') || pathToInitial.endsWith('.bat')

    scriptFile = resolve(tmpdir(), `${fileName}.cmd`)
    script += '@echo off\n'
    script += cmd
    if (args.length) {
      script += ` ${args.map((arg) => escape.cmd(arg, doubleEscape)).join(' ')}`
    }
  } else {
    scriptFile = resolve(tmpdir(), `${fileName}.sh`)
    script = cmd
    if (args.length) {
      script += ` ${args.map((arg) => escape.sh(arg)).join(' ')}`
    }
  }

  writeFile(scriptFile, script)
  const spawnArgs = isCmd
    ? ['/d', '/s', '/c', escape.cmd(scriptFile)]
    : [isWindows ? translateWinPathToPosix(scriptFile) : scriptFile]

  const spawnOpts = {
    env: spawnEnv,
    stdioString,
    stdio,
    cwd: path,
    ...(isCmd ? { windowsVerbatimArguments: true } : {}),
  }

  const cleanup = () => {
    // delete the script, this is just a best effort
    try {
      unlink(scriptFile)
    } catch (err) {
      // ignore errors
    }
  }

  return [scriptShell, spawnArgs, spawnOpts, cleanup]
}

module.exports = makeSpawnArgs
