const util = require('util')
const Arborist = require('@npmcli/arborist')
const rimraf = util.promisify(require('rimraf'))
const reifyFinish = require('../utils/reify-finish.js')
const runScript = require('@npmcli/run-script')
const fs = require('fs')
const readdir = util.promisify(fs.readdir)
const log = require('../utils/log-shim.js')
const validateLockfile = require('../utils/validate-lockfile.js')

const removeNodeModules = async where => {
  const rimrafOpts = { glob: false }
  process.emit('time', 'npm-ci:rm')
  const path = `${where}/node_modules`
  // get the list of entries so we can skip the glob for performance
  const entries = await readdir(path, null).catch(er => [])
  await Promise.all(entries.map(f => rimraf(`${path}/${f}`, rimrafOpts)))
  process.emit('timeEnd', 'npm-ci:rm')
}
const ArboristWorkspaceCmd = require('../arborist-cmd.js')

class CI extends ArboristWorkspaceCmd {
  static description = 'Install a project with a clean slate'
  static name = 'ci'
  static params = [
    'audit',
    'ignore-scripts',
    'script-shell',
  ]

  async exec () {
    if (this.npm.config.get('global')) {
      const err = new Error('`npm ci` does not work for global packages')
      err.code = 'ECIGLOBAL'
      throw err
    }

    const where = this.npm.prefix
    const opts = {
      ...this.npm.flatOptions,
      packageLock: true, // npm ci should never skip lock files
      path: where,
      log,
      save: false, // npm ci should never modify the lockfile or package.json
      workspaces: this.workspaceNames,
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

    // retrieves inventory of packages from loaded virtual tree (lock file)
    const virtualInventory = new Map(arb.virtualTree.inventory)

    // build ideal tree step needs to come right after retrieving the virtual
    // inventory since it's going to erase the previous ref to virtualTree
    await arb.buildIdealTree()

    // verifies that the packages from the ideal tree will match
    // the same versions that are present in the virtual tree (lock file)
    // throws a validation error in case of mismatches
    const errors = validateLockfile(virtualInventory, arb.idealTree.inventory)
    if (errors.length) {
      throw new Error(
        '`npm ci` can only install packages when your package.json and ' +
        'package-lock.json or npm-shrinkwrap.json are in sync. Please ' +
        'update your lock file with `npm install` ' +
        'before continuing.\n\n' +
        errors.join('\n') + '\n'
      )
    }

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
