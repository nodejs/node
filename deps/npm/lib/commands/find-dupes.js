// dedupe duplicated packages, or find them in the tree
const ArboristWorkspaceCmd = require('../arborist-cmd.js')

class FindDupes extends ArboristWorkspaceCmd {
  static description = 'Find duplication in the package tree'
  static name = 'find-dupes'
  static params = [
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

  async exec (args, cb) {
    this.npm.config.set('dry-run', true)
    return this.npm.exec('dedupe', [])
  }
}
module.exports = FindDupes
