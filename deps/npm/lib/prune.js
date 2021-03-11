// prune extraneous packages
const Arborist = require('@npmcli/arborist')
const reifyFinish = require('./utils/reify-finish.js')

const BaseCommand = require('./base-command.js')
class Prune extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'prune'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg>...] [--production]']
  }

  exec (args, cb) {
    this.prune().then(() => cb()).catch(cb)
  }

  async prune () {
    const where = this.npm.prefix
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
    })
    await arb.prune(this.npm.flatOptions)
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Prune
