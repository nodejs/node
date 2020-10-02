// remove a package.

const Arborist = require('@npmcli/arborist')
const npm = require('./npm.js')
const rpj = require('read-package-json-fast')
const { resolve } = require('path')
const usageUtil = require('./utils/usage.js')
const reifyOutput = require('./utils/reify-output.js')

const cmd = (args, cb) => rm(args).then(() => cb()).catch(cb)

const rm = async args => {
  // the /path/to/node_modules/..
  const { global, prefix } = npm.flatOptions
  const path = global ? resolve(npm.globalDir, '..') : prefix

  if (!args.length) {
    if (!global) {
      throw new Error('must provide a package name to remove')
    } else {
      const pkg = await rpj(resolve(npm.localPrefix, 'package.json'))
        .catch(er => {
          throw er.code !== 'ENOENT' && er.code !== 'ENOTDIR' ? er : usage()
        })
      args.push(pkg.name)
    }
  }

  const arb = new Arborist({ ...npm.flatOptions, path })

  await arb.reify({
    ...npm.flatOptions,
    rm: args
  })
  reifyOutput(arb)
}

const usage = usageUtil(
  'uninstall',
  'npm uninstall [<@scope>/]<pkg>[@<version>]... [--save-prod|--save-dev|--save-optional] [--no-save]'
)

const completion = require('./utils/completion/installed-shallow.js')

module.exports = Object.assign(cmd, { usage, completion })
