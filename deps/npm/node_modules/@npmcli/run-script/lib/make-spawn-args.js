const setPATH = require('./set-path.js')
const { resolve } = require('path')

let npmConfigNodeGyp

const makeSpawnArgs = options => {
  const {
    args,
    binPaths,
    cmd,
    env,
    event,
    nodeGyp,
    path,
    scriptShell = true,
    stdio,
    stdioString,
  } = options

  if (nodeGyp) {
    // npm already pulled this from env and passes it in to options
    npmConfigNodeGyp = nodeGyp
  } else if (env.npm_config_node_gyp) {
    // legacy mode for standalone user
    npmConfigNodeGyp = env.npm_config_node_gyp
  } else {
    // default
    npmConfigNodeGyp = require.resolve('node-gyp/bin/node-gyp.js')
  }

  const spawnEnv = setPATH(path, binPaths, {
    // we need to at least save the PATH environment var
    ...process.env,
    ...env,
    npm_package_json: resolve(path, 'package.json'),
    npm_lifecycle_event: event,
    npm_lifecycle_script: cmd,
    npm_config_node_gyp: npmConfigNodeGyp,
  })

  const spawnOpts = {
    env: spawnEnv,
    stdioString,
    stdio,
    cwd: path,
    shell: scriptShell,
  }

  return [cmd, args, spawnOpts]
}

module.exports = makeSpawnArgs
