const util = require('util')
const Arborist = require('@npmcli/arborist')
const rimraf = util.promisify(require('rimraf'))
const reifyFinish = require('./utils/reify-finish.js')
const runScript = require('@npmcli/run-script')
const fs = require('fs')
const readdir = util.promisify(fs.readdir)

const log = require('npmlog')
const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil('ci', 'npm ci')

const cmd = (args, cb) => ci().then(() => cb()).catch(cb)

const removeNodeModules = async where => {
  const rimrafOpts = { glob: false }
  process.emit('time', 'npm-ci:rm')
  const path = `${where}/node_modules`
  // get the list of entries so we can skip the glob for performance
  const entries = await readdir(path, null).catch(er => [])
  await Promise.all(entries.map(f => rimraf(`${path}/${f}`, rimrafOpts)))
  process.emit('timeEnd', 'npm-ci:rm')
}

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
    removeNodeModules(where),
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
        banner: log.level !== 'silent',
        event,
      })
    }
  }
  await reifyFinish(arb)
}

module.exports = Object.assign(cmd, {usage})
