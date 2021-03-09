const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const rpj = require('read-package-json-fast')

const reifyFinish = require('./utils/reify-finish.js')
const completion = require('./utils/completion/installed-shallow.js')

const BaseCommand = require('./base-command.js')
class Uninstall extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'uninstall'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<@scope>/]<pkg>[@<version>]... [-S|--save|--no-save]']
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
    const { global, prefix } = this.npm.flatOptions
    const path = global ? resolve(this.npm.globalDir, '..') : prefix

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

    const arb = new Arborist({ ...this.npm.flatOptions, path })

    await arb.reify({
      ...this.npm.flatOptions,
      rm: args,
    })
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Uninstall
