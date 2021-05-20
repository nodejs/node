// dedupe duplicated packages, or find them in the tree
const ArboristWorkspaceCmd = require('./workspaces/arborist-cmd.js')

class FindDupes extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Find duplication in the package tree'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'find-dupes'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'global-style',
      'legacy-bundling',
      'strict-peer-deps',
      'package-lock',
      'omit',
      'ignore-scripts',
      'audit',
      'bin-links',
      'fund',
      ...super.params,
    ]
  }

  exec (args, cb) {
    this.npm.config.set('dry-run', true)
    this.npm.commands.dedupe([], cb)
  }
}
module.exports = FindDupes
