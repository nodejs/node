// dedupe duplicated packages, or find them in the tree
const BaseCommand = require('./base-command.js')

class FindDupes extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'find-dupes'
  }

  exec (args, cb) {
    this.npm.config.set('dry-run', true)
    this.npm.commands.dedupe([], cb)
  }
}
module.exports = FindDupes
