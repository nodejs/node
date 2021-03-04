// dedupe duplicated packages, or find them in the tree
const usageUtil = require('./utils/usage.js')

class FindDupes {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('find-dupes', 'npm find-dupes')
  }

  exec (args, cb) {
    this.npm.config.set('dry-run', true)
    this.npm.commands.dedupe([], cb)
  }
}
module.exports = FindDupes
