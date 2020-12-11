const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const rpj = require('read-package-json-fast')

const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const reifyFinish = require('./utils/reify-finish.js')
const completion = require('./utils/completion/installed-shallow.js')

const usage = usageUtil(
  'uninstall',
  'npm uninstall [<@scope>/]<pkg>[@<version>]... [--save-prod|--save-dev|--save-optional] [--no-save]'
)

const cmd = (args, cb) => rm(args).then(() => cb()).catch(cb)

const rm = async args => {
  // the /path/to/node_modules/..
  const { global, prefix } = npm.flatOptions
  const path = global ? resolve(npm.globalDir, '..') : prefix

  if (!args.length) {
    if (!global)
      throw new Error('Must provide a package name to remove')
    else {
      let pkg

      try {
        pkg = await rpj(resolve(npm.localPrefix, 'package.json'))
      } catch (er) {
        if (er.code !== 'ENOENT' && er.code !== 'ENOTDIR')
          throw er
        else
          throw usage
      }

      args.push(pkg.name)
    }
  }

  const arb = new Arborist({ ...npm.flatOptions, path })

  await arb.reify({
    ...npm.flatOptions,
    rm: args,
  })
  await reifyFinish(arb)
}

module.exports = Object.assign(cmd, { usage, completion })
