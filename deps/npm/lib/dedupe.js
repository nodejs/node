// dedupe duplicated packages, or find them in the tree
const Arborist = require('@npmcli/arborist')
const reifyFinish = require('./utils/reify-finish.js')

const BaseCommand = require('./base-command.js')

class Dedupe extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Reduce duplication in the package tree'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'dedupe'
  }

  exec (args, cb) {
    this.dedupe(args).then(() => cb()).catch(cb)
  }

  async dedupe (args) {
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
    }
    const arb = new Arborist(opts)
    await arb.dedupe(opts)
    await reifyFinish(this.npm, arb)
  }
}

module.exports = Dedupe
