const path = require('path')

const Arborist = require('@npmcli/arborist')
const log = require('npmlog')

const reifyFinish = require('../utils/reify-finish.js')
const completion = require('../utils/completion/installed-deep.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
class Update extends ArboristWorkspaceCmd {
  static description = 'Update packages'
  static name = 'update'
  static params = [
    'global',
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

  static usage = ['[<pkg>...]']

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  async exec (args) {
    const update = args.length === 0 ? true : args
    const global = path.resolve(this.npm.globalDir, '..')
    const where = this.npm.config.get('global')
      ? global
      : this.npm.prefix

    if (this.npm.config.get('depth')) {
      log.warn('update', 'The --depth option no longer has any effect. See RFC0019.\n' +
        'https://github.com/npm/rfcs/blob/latest/implemented/0019-remove-update-depth-option.md')
    }

    const arb = new Arborist({
      ...this.npm.flatOptions,
      log: this.npm.log,
      path: where,
      workspaces: this.workspaceNames,
    })

    await arb.reify({ update })
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Update
