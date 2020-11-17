// prune extraneous packages
const npm = require('./npm.js')
const Arborist = require('@npmcli/arborist')
const usageUtil = require('./utils/usage.js')

const reifyFinish = require('./utils/reify-finish.js')

const usage = usageUtil('prune',
  'npm prune [[<@scope>/]<pkg>...] [--production]'
)
const completion = require('./utils/completion/none.js')

const cmd = (args, cb) => prune().then(() => cb()).catch(cb)

const prune = async () => {
  const where = npm.prefix
  const arb = new Arborist({
    ...npm.flatOptions,
    path: where,
  })
  await arb.prune(npm.flatOptions)
  await reifyFinish(arb)
}

module.exports = Object.assign(cmd, { usage, completion })
