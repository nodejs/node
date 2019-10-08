'use strict'

const npm = require('./npm.js')
const Installer = require('libcipm')
const log = require('npmlog')
const path = require('path')
const pack = require('./pack.js')

ci.usage = 'npm ci'

ci.completion = (cb) => cb(null, [])

module.exports = ci
function ci (args, cb) {
  const opts = {
    // Add some non-npm-config opts by hand.
    cache: path.join(npm.config.get('cache'), '_cacache'),
    // NOTE: npm has some magic logic around color distinct from the config
    // value, so we have to override it here
    color: !!npm.color,
    hashAlgorithm: 'sha1',
    includeDeprecated: false,
    log,
    'npm-session': npm.session,
    'project-scope': npm.projectScope,
    refer: npm.referer,
    dmode: npm.modes.exec,
    fmode: npm.modes.file,
    umask: npm.modes.umask,
    npmVersion: npm.version,
    tmp: npm.tmp,
    dirPacker: pack.packGitDep
  }

  for (const key in npm.config.list[0]) {
    if (key !== 'log') {
      opts[key] = npm.config.list[0][key]
    }
  }

  return new Installer(opts).run().then(details => {
    log.disableProgress()
    console.log(`added ${details.pkgCount} packages in ${
      details.runTime / 1000
    }s`)
  }).then(() => cb(), cb)
}
