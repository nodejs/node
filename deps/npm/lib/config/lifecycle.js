'use strict'

const npm = require('../npm.js')
const log = require('npmlog')

module.exports = lifecycleOpts

let opts

function lifecycleOpts (moreOpts) {
  if (!opts) {
    opts = {
      config: npm.config.snapshot,
      dir: npm.dir,
      failOk: false,
      force: npm.config.get('force'),
      group: npm.config.get('group'),
      ignorePrepublish: npm.config.get('ignore-prepublish'),
      ignoreScripts: npm.config.get('ignore-scripts'),
      log: log,
      nodeOptions: npm.config.get('node-options'),
      production: npm.config.get('production'),
      scriptShell: npm.config.get('script-shell'),
      scriptsPrependNodePath: npm.config.get('scripts-prepend-node-path'),
      unsafePerm: npm.config.get('unsafe-perm'),
      user: npm.config.get('user')
    }
  }

  return moreOpts ? Object.assign({}, opts, moreOpts) : opts
}
