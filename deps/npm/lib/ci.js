const util = require('util')
const Arborist = require('@npmcli/arborist')
const rimraf = util.promisify(require('rimraf'))
const reifyOutput = require('./utils/reify-output.js')

const log = require('npmlog')
const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil('ci', 'npm ci')
const completion = require('./utils/completion/none.js')

const cmd = (args, cb) => ci().then(() => cb()).catch(cb)

const ci = async () => {
  if (npm.flatOptions.global) {
    const err = new Error('`npm ci` does not work for global packages')
    err.code = 'ECIGLOBAL'
    throw err
  }

  const where = npm.prefix
  const arb = new Arborist({ ...npm.flatOptions, path: where })

  await Promise.all([
    arb.loadVirtual().catch(er => {
      log.verbose('loadVirtual', er.stack)
      const msg =
        'The `npm ci` command can only install with an existing package-lock.json or\n' +
        'npm-shrinkwrap.json with lockfileVersion >= 1. Run an install with npm@5 or\n' +
        'later to generate a package-lock.json file, then try again.'
      throw new Error(msg)
    }),
    rimraf(`${where}/node_modules/`)
  ])
  // npm ci should never modify the lockfile or package.json
  await arb.reify({ save: false })
  reifyOutput(arb)
}

module.exports = Object.assign(cmd, { completion, usage })
