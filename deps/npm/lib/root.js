module.exports = root

var npm = require('./npm.js')

root.usage = 'npm root [-g]'

function root (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  if (!silent) console.log(npm.dir)
  process.nextTick(cb.bind(this, null, npm.dir))
}
