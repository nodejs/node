const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const rpj = require('read-package-json-fast')

const reifyFinish = require('../utils/reify-finish.js')
const completion = require('../utils/completion/installed-shallow.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
class Uninstall extends ArboristWorkspaceCmd {
  static description = 'Remove a package'
  static name = 'uninstall'
  static params = ['save', ...super.params]
  static usage = ['[<@scope>/]<pkg>...']
  static ignoreImplicitWorkspace = false

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  async exec (args) {
    if (!args.length) {
      if (!this.npm.global) {
        throw new Error('Must provide a package name to remove')
      } else {
        try {
          const pkg = await rpj(resolve(this.npm.localPrefix, 'package.json'))
          args.push(pkg.name)
        } catch (er) {
          if (er.code !== 'ENOENT' && er.code !== 'ENOTDIR') {
            throw er
          } else {
            throw this.usageError()
          }
        }
      }
    }

    // the /path/to/node_modules/..
    const path = this.npm.global
      ? resolve(this.npm.globalDir, '..')
      : this.npm.localPrefix

    const opts = {
      ...this.npm.flatOptions,
      path,
      rm: args,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)
    await arb.reify(opts)
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Uninstall
