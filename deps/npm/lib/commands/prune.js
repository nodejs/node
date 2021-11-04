// prune extraneous packages
const Arborist = require('@npmcli/arborist')
const reifyFinish = require('../utils/reify-finish.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
class Prune extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Remove extraneous packages'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'prune'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['omit', 'dry-run', 'json', ...super.params]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg>...]']
  }

  async exec () {
    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      path: where,
      log: this.npm.log,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)
    await arb.prune(opts)
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Prune
