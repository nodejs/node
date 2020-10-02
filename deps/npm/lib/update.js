const path = require('path')
const Arborist = require('@npmcli/arborist')

const log = require('npmlog')
const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const reifyOutput = require('./utils/reify-output.js')
const completion = require('./utils/completion/installed-deep.js')

const usage = usageUtil(
  'update',
  'npm update [-g] [<pkg>...]'
)

const cmd = (args, cb) => update(args).then(() => cb()).catch(cb)

const update = async args => {
  const update = args.length === 0 ? true : args
  const global = path.resolve(npm.globalDir, '..')
  const where = npm.flatOptions.global
    ? global
    : npm.prefix

  if (npm.flatOptions.depth) {
    log.warn('update', 'The --depth option no longer has any effect. See RFC0019.\n' +
      'https://github.com/npm/rfcs/blob/latest/accepted/0019-remove-update-depth-option.md')
  }

  const arb = new Arborist({
    ...npm.flatOptions,
    path: where
  })

  await arb.reify({ update })
  reifyOutput(arb)
}

module.exports = Object.assign(cmd, { usage, completion })
