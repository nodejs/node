module.exports = root

var npm = require('./npm.js')
var output = require('./utils/output.js')

root.usage = 'npm root [-g]'

function root (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  if (!silent) output(npm.dir)
  process.nextTick(cb.bind(this, null, npm.dir))
}
