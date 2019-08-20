'use strict'

const Installer = require('libcipm')
const npmConfig = require('./config/figgy-config.js')
const npmlog = require('npmlog')

ci.usage = 'npm ci'

ci.completion = (cb) => cb(null, [])

module.exports = ci
function ci (args, cb) {
  return new Installer(npmConfig({ log: npmlog })).run().then(details => {
    npmlog.disableProgress()
    console.log(`added ${details.pkgCount} packages in ${
      details.runTime / 1000
    }s`)
  }).then(() => cb(), cb)
}
