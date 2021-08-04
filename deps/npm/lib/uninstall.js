const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const rpj = require('read-package-json-fast')

const reifyFinish = require('./utils/reify-finish.js')
const completion = require('./utils/completion/installed-shallow.js')

const ArboristWorkspaceCmd = require('./workspaces/arborist-cmd.js')
class Uninstall extends ArboristWorkspaceCmd {
  static get description () {
    return 'Remove a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'uninstall'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['save', ...super.params]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<@scope>/]<pkg>...']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  exec (args, cb) {
    this.uninstall(args).then(() => cb()).catch(cb)
  }

  async uninstall (args) {
    // the /path/to/node_modules/..
    const global = this.npm.config.get('global')
    const path = global
      ? resolve(this.npm.globalDir, '..')
      : this.npm.localPrefix

    if (!args.length) {
      if (!global)
        throw new Error('Must provide a package name to remove')
      else {
        let pkg

        try {
          pkg = await rpj(resolve(this.npm.localPrefix, 'package.json'))
        } catch (er) {
          if (er.code !== 'ENOENT' && er.code !== 'ENOTDIR')
            throw er
          else
            throw this.usage
        }

        args.push(pkg.name)
      }
    }

    const opts = {
      ...this.npm.flatOptions,
      path,
      log: this.npm.log,
      rm: args,
      workspaces: this.workspaceNames,
    }
    const arb = new Arborist(opts)
    await arb.reify(opts)
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Uninstall
