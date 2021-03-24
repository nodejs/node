// prune extraneous packages
const Arborist = require('@npmcli/arborist')
const reifyFinish = require('./utils/reify-finish.js')

const BaseCommand = require('./base-command.js')
class Prune extends BaseCommand {
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
    return ['production']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg>...]']
  }

  exec (args, cb) {
    this.prune().then(() => cb()).catch(cb)
  }

  async prune () {
    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      path: where,
      log: this.npm.log,
    }
    const arb = new Arborist(opts)
    await arb.prune(opts)
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Prune
