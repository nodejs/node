// dedupe duplicated packages, or find them in the tree
const npm = require('./npm.js')
const Arborist = require('@npmcli/arborist')
const usageUtil = require('./utils/usage.js')
const reifyFinish = require('./utils/reify-finish.js')

const usage = usageUtil('dedupe', 'npm dedupe')
const completion = require('./utils/completion/none.js')

const cmd = (args, cb) => dedupe(args).then(() => cb()).catch(cb)

const dedupe = async (args) => {
  const dryRun = (args && args.dryRun) || npm.flatOptions.dryRun
  const where = npm.prefix
  const arb = new Arborist({
    ...npm.flatOptions,
    path: where,
    dryRun,
  })
  await arb.dedupe(npm.flatOptions)
  await reifyFinish(arb)
}

module.exports = Object.assign(cmd, { usage, completion })
