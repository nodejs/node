const path = require('path')

const Arborist = require('@npmcli/arborist')
const log = require('../utils/log-shim.js')

const reifyFinish = require('../utils/reify-finish.js')
const completion = require('../utils/completion/installed-deep.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
class Update extends ArboristWorkspaceCmd {
  static description = 'Update packages'
  static name = 'update'

  static params = [
    'save',
    'global',
    'install-strategy',
    'legacy-bundling',
    'global-style',
    'omit',
    'strict-peer-deps',
    'package-lock',
    'foreground-scripts',
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
    const where = this.npm.global
      ? global
      : this.npm.prefix

    // In the context of `npm update` the save
    // config value should default to `false`
    const save = this.npm.config.isDefault('save')
      ? false
      : this.npm.config.get('save')

    if (this.npm.config.get('depth')) {
      log.warn('update', 'The --depth option no longer has any effect. See RFC0019.\n' +
        'https://github.com/npm/rfcs/blob/latest/implemented/0019-remove-update-depth-option.md')
    }

    const opts = {
      ...this.npm.flatOptions,
      path: where,
      save,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)

    await arb.reify({ ...opts, update })
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Update
