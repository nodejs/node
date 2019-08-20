'use strict'

const npm = require('./npm.js')
const Installer = require('libcipm')
const npmlog = require('npmlog')

ci.usage = 'npm ci'

ci.completion = (cb) => cb(null, [])

module.exports = ci
function ci (args, cb) {
  const opts = Object.create({ log: npmlog })
  for (const key in npm.config.list[0]) {
    if (key !== 'log') {
      opts[key] = npm.config.list[0][key]
    }
  }
  return new Installer(opts).run().then(details => {
    npmlog.disableProgress()
    console.log(`added ${details.pkgCount} packages in ${
      details.runTime / 1000
    }s`)
  }).then(() => cb(), cb)
}
