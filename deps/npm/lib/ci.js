const util = require('util')
const Arborist = require('@npmcli/arborist')
const rimraf = util.promisify(require('rimraf'))
const reifyFinish = require('./utils/reify-finish.js')
const runScript = require('@npmcli/run-script')

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
  const { scriptShell, ignoreScripts } = npm.flatOptions
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
    rimraf(`${where}/node_modules/*`, { glob: { dot: true, nosort: true, silent: true } }),
  ])
  // npm ci should never modify the lockfile or package.json
  await arb.reify({ ...npm.flatOptions, save: false })

  // run the same set of scripts that `npm install` runs.
  if (!ignoreScripts) {
    const scripts = [
      'preinstall',
      'install',
      'postinstall',
      'prepublish', // XXX should we remove this finally??
      'preprepare',
      'prepare',
      'postprepare',
    ]
    for (const event of scripts) {
      await runScript({
        path: where,
        args: [],
        scriptShell,
        stdio: 'inherit',
        stdioString: true,
        event,
      })
    }
  }
  await reifyFinish(arb)
}

module.exports = Object.assign(cmd, { completion, usage })
