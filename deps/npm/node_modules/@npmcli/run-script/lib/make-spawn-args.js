/* eslint camelcase: "off" */
const isWindows = require('./is-windows.js')
const setPATH = require('./set-path.js')
const { resolve } = require('path')
const which = require('which')
const npm_config_node_gyp = require.resolve('node-gyp/bin/node-gyp.js')
const escape = require('./escape.js')

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

  let doubleEscape = false
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

    doubleEscape = pathToInitial.endsWith('.cmd') || pathToInitial.endsWith('.bat')
  }

  let script = cmd
  for (const arg of args) {
    script += isCmd
      ? ` ${escape.cmd(arg, doubleEscape)}`
      : ` ${escape.sh(arg)}`
  }
  const spawnArgs = isCmd
    ? ['/d', '/s', '/c', script]
    : ['-c', '--', script]

  const spawnOpts = {
    env: spawnEnv,
    stdioString,
    stdio,
    cwd: path,
    ...(isCmd ? { windowsVerbatimArguments: true } : {}),
  }

  return [scriptShell, spawnArgs, spawnOpts]
}

module.exports = makeSpawnArgs
