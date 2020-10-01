const isWindows = require('./is-windows.js')
const setPATH = require('./set-path.js')
const {resolve} = require('path')
const npm_config_node_gyp = require.resolve('node-gyp/bin/node-gyp.js')

const makeSpawnArgs = options => {
  const {
    event,
    path,
    scriptShell = isWindows ? process.env.comspec || 'cmd' : 'sh',
    env = {},
    stdio,
    cmd,
    stdioString = false,
  } = options

  const isCmd = /(?:^|\\)cmd(?:\.exe)?$/i.test(scriptShell)
  const args = isCmd ? ['/d', '/s', '/c', `"${cmd}"`] : ['-c', cmd]

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

  return [scriptShell, args, spawnOpts]
}

module.exports = makeSpawnArgs
