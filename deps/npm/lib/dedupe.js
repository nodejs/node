// dedupe duplicated packages, or find them in the tree
const Arborist = require('@npmcli/arborist')
const usageUtil = require('./utils/usage.js')
const reifyFinish = require('./utils/reify-finish.js')

class Dedupe {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('dedupe', 'npm dedupe')
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
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
      dryRun,
    })
    await arb.dedupe(this.npm.flatOptions)
    await reifyFinish(this.npm, arb)
  }
}

module.exports = Dedupe
