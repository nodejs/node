'use strict'

const log = require('npmlog')

module.exports = lifecycleOpts
function lifecycleOpts (opts) {
  const objConfig = {}
  for (const key of opts.keys()) {
    const val = opts.get(key)
    if (val != null) {
      objConfig[key] = val
    }
  }
  return {
    config: objConfig,
    scriptShell: opts.get('script-shell'),
    force: opts.get('force'),
    user: opts.get('user'),
    group: opts.get('group'),
    ignoreScripts: opts.get('ignore-scripts'),
    ignorePrepublish: opts.get('ignore-prepublish'),
    scriptsPrependNodePath: opts.get('scripts-prepend-node-path'),
    unsafePerm: opts.get('unsafe-perm'),
    log,
    dir: opts.get('prefix'),
    failOk: false,
    production: opts.get('production')
  }
}
