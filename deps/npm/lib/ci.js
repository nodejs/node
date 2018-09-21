'use strict'

const Installer = require('libcipm')
const lifecycleOpts = require('./config/lifecycle.js')
const npm = require('./npm.js')
const npmlog = require('npmlog')
const pacoteOpts = require('./config/pacote.js')

ci.usage = 'npm ci'

ci.completion = (cb) => cb(null, [])

Installer.CipmConfig.impl(npm.config, {
  get: npm.config.get,
  set: npm.config.set,
  toLifecycle (moreOpts) {
    return lifecycleOpts(moreOpts)
  },
  toPacote (moreOpts) {
    return pacoteOpts(moreOpts)
  }
})

module.exports = ci
function ci (args, cb) {
  return new Installer({
    config: npm.config,
    log: npmlog
  })
    .run()
    .then(
      (details) => {
        npmlog.disableProgress()
        console.error(`added ${details.pkgCount} packages in ${
          details.runTime / 1000
        }s`)
      }
    )
    .then(() => cb(), cb)
}
