// dedupe duplicated packages, or find them in the tree
const Arborist = require('@npmcli/arborist')
const reifyFinish = require('../utils/reify-finish.js')
const log = require('../utils/log-shim.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')

class Dedupe extends ArboristWorkspaceCmd {
  static description = 'Reduce duplication in the package tree'
  static name = 'dedupe'
  static params = [
    'global-style',
    'legacy-bundling',
    'strict-peer-deps',
    'package-lock',
    'save',
    'omit',
    'ignore-scripts',
    'audit',
    'bin-links',
    'fund',
    'dry-run',
    ...super.params,
  ]

  async exec (args) {
    if (this.npm.config.get('global')) {
      const er = new Error('`npm dedupe` does not work in global mode.')
      er.code = 'EDEDUPEGLOBAL'
      throw er
    }

    // In the context of `npm dedupe` the save
    // config value should default to `false`
    const save = this.npm.config.isDefault('save')
      ? false
      : this.npm.config.get('save')

    const dryRun = this.npm.config.get('dry-run')
    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      log,
      path: where,
      dryRun,
      save,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)
    await arb.dedupe(opts)
    await reifyFinish(this.npm, arb)
  }
}

module.exports = Dedupe
