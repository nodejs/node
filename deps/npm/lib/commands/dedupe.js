// dedupe duplicated packages, or find them in the tree
const Arborist = require('@npmcli/arborist')
const reifyFinish = require('../utils/reify-finish.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')

class Dedupe extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Reduce duplication in the package tree'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'dedupe'
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
      'dry-run',
      ...super.params,
    ]
  }

  async exec (args) {
    if (this.npm.config.get('global')) {
      const er = new Error('`npm dedupe` does not work in global mode.')
      er.code = 'EDEDUPEGLOBAL'
      throw er
    }

    const dryRun = this.npm.config.get('dry-run')
    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      log: this.npm.log,
      path: where,
      dryRun,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)
    await arb.dedupe(opts)
    await reifyFinish(this.npm, arb)
  }
}

module.exports = Dedupe
