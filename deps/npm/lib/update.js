const path = require('path')

const Arborist = require('@npmcli/arborist')
const log = require('npmlog')

const reifyFinish = require('./utils/reify-finish.js')
const completion = require('./utils/completion/installed-deep.js')

const BaseCommand = require('./base-command.js')
class Update extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'update'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[-g] [<pkg>...]']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  exec (args, cb) {
    this.update(args).then(() => cb()).catch(cb)
  }

  async update (args) {
    const update = args.length === 0 ? true : args
    const global = path.resolve(this.npm.globalDir, '..')
    const where = this.npm.flatOptions.global
      ? global
      : this.npm.prefix

    if (this.npm.flatOptions.depth) {
      log.warn('update', 'The --depth option no longer has any effect. See RFC0019.\n' +
        'https://github.com/npm/rfcs/blob/latest/implemented/0019-remove-update-depth-option.md')
    }

    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
    })

    await arb.reify({ update })
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Update
