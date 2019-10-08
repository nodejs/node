const t = require('tap')
if (!process.getuid || process.getuid() !== 0 || !process.env.SUDO_UID || !process.env.SUDO_GID) {
  t.pass('this test only runs in sudo mode')
  t.end()
  process.exit(0)
}

const common = require('../common-tap.js')
const fs = require('fs')
const mkdirp = require('mkdirp')
mkdirp.sync(common.cache + '/root/owned')
fs.writeFileSync(common.cache + '/root/owned/file.txt', 'should be chowned')
const chown = require('chownr')

// this will fire after t.teardown() but before process.on('exit')
setTimeout(() => {
  chown.sync(common.cache, +process.env.SUDO_UID, +process.env.SUDO_GID)
}, 100)

t.pass('this is fine')
