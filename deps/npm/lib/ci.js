const util = require('util')
const Arborist = require('@npmcli/arborist')
const rimraf = util.promisify(require('rimraf'))
const reifyFinish = require('./utils/reify-finish.js')
const runScript = require('@npmcli/run-script')
const fs = require('fs')
const readdir = util.promisify(fs.readdir)

const log = require('npmlog')

const removeNodeModules = async where => {
  const rimrafOpts = { glob: false }
  process.emit('time', 'npm-ci:rm')
  const path = `${where}/node_modules`
  // get the list of entries so we can skip the glob for performance
  const entries = await readdir(path, null).catch(er => [])
  await Promise.all(entries.map(f => rimraf(`${path}/${f}`, rimrafOpts)))
  process.emit('timeEnd', 'npm-ci:rm')
}
const ArboristWorkspaceCmd = require('./workspaces/arborist-cmd.js')

class CI extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Install a project with a clean slate'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'ci'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'ignore-scripts',
      'script-shell',
    ]
  }

  exec (args, cb) {
    this.ci().then(() => cb()).catch(cb)
  }

  async ci () {
    if (this.npm.config.get('global')) {
      const err = new Error('`npm ci` does not work for global packages')
      err.code = 'ECIGLOBAL'
      throw err
    }

    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      path: where,
      log: this.npm.log,
      save: false, // npm ci should never modify the lockfile or package.json
      workspaces: this.workspaces,
    }

    const arb = new Arborist(opts)
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
    await arb.reify(opts)

    const ignoreScripts = this.npm.config.get('ignore-scripts')
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
      const scriptShell = this.npm.config.get('script-shell') || undefined
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
    await reifyFinish(this.npm, arb)
  }
}

module.exports = CI
