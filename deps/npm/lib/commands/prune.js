// prune extraneous packages
const Arborist = require('@npmcli/arborist')
const log = require('../utils/log-shim.js')
const reifyFinish = require('../utils/reify-finish.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
class Prune extends ArboristWorkspaceCmd {
  static description = 'Remove extraneous packages'
  static name = 'prune'
  static params = ['omit', 'dry-run', 'json', ...super.params]
  static usage = ['[[<@scope>/]<pkg>...]']

  async exec () {
    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      path: where,
      log,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)
    await arb.prune(opts)
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Prune
