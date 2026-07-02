const reifyFinish = require('../utils/reify-finish.js')
const resolveAllowScripts = require('../utils/resolve-allow-scripts.js')
const strictAllowScriptsPreflight = require('../utils/strict-allow-scripts-preflight.js')
const runScript = require('@npmcli/run-script')
const fs = require('node:fs/promises')
const path = require('node:path')
const { log, time } = require('proc-log')
const validateLockfile = require('../utils/validate-lockfile.js')
const { validatePackageExtensions } = require('../utils/validate-lockfile.js')
const ArboristWorkspaceCmd = require('../arborist-cmd.js')
const getWorkspaces = require('../utils/get-workspaces.js')

class CI extends ArboristWorkspaceCmd {
  static description = 'Clean install a project'
  static name = 'ci'

  // These are in the order they will show up in when running "-h"
  static params = [
    'install-strategy',
    'legacy-bundling',
    'global-style',
    'omit',
    'include',
    'strict-peer-deps',
    'foreground-scripts',
    'ignore-scripts',
    'allow-directory',
    'allow-file',
    'allow-git',
    'allow-remote',
    'allow-scripts',
    'strict-allow-scripts',
    'dangerously-allow-all-scripts',
    'audit',
    'bin-links',
    'fund',
    'dry-run',
    ...super.params,
  ]

  async exec () {
    if (this.npm.global) {
      throw Object.assign(new Error('`npm ci` does not work for global packages'), {
        code: 'ECIGLOBAL',
      })
    }

    // npm ci is always strict about patches; the relax flags are not accepted
    for (const flag of ['allow-unused-patches', 'ignore-patch-failures']) {
      if (this.npm.config.find(flag) === 'cli') {
        throw Object.assign(new Error(`The --${flag} flag is not allowed with \`npm ci\`.`), {
          code: 'ECIPATCHFLAG',
        })
      }
    }

    const dryRun = this.npm.config.get('dry-run')
    const ignoreScripts = this.npm.config.get('ignore-scripts')
    const where = this.npm.prefix
    const Arborist = require('@npmcli/arborist')
    const { policy: allowScriptsPolicy } = await resolveAllowScripts(this.npm)
    const opts = {
      ...this.npm.flatOptions,
      packageLock: true, // npm ci should never skip lock files
      path: where,
      save: false, // npm ci should never modify the lockfile or package.json
      workspaces: this.workspaceNames,
      allowScripts: allowScriptsPolicy,
    }

    // generate an inventory from the virtual tree in the lockfile
    const virtualArb = new Arborist(opts)
    try {
      await virtualArb.loadVirtual()
    } catch (err) {
      log.verbose('loadVirtual', err.stack)
      const msg =
        'The `npm ci` command can only install with an existing\n' +
        'package-lock.json with lockfileVersion >= 1. Run an install with npm@5\n' +
        'or later to generate a package-lock.json file, then try again.'
      throw this.usageError(msg)
    }
    const virtualInventory = new Map(virtualArb.virtualTree.inventory)

    // Now we make our real Arborist.
    // We need a new one because the virtual tree from the lockfile can have extraneous dependencies in it that won't install on this platform
    const arb = new Arborist(opts)
    await arb.buildIdealTree()
    await strictAllowScriptsPreflight({ arb, npm: this.npm, idealTreeOpts: opts })

    // Verifies that the packages from the ideal tree will match the same versions that are present in the virtual tree (lock file).
    const errors = validateLockfile(virtualInventory, arb.idealTree.inventory)
    // Verifies that the root packageExtensions state matches the lockfile and is still consistent with the locked tree.
    errors.push(...validatePackageExtensions(virtualArb.virtualTree, arb.idealTree))
    if (errors.length) {
      throw this.usageError(
        '`npm ci` can only install packages when your package.json and package-lock.json are in sync. ' +
        'Please update your lock file with `npm install` before continuing.\n\n' +
        errors.join('\n')
      )
    }

    if (!dryRun) {
      const workspacePaths = await getWorkspaces([], {
        path: this.npm.localPrefix,
        includeWorkspaceRoot: true,
      })

      // Only remove node_modules after we've successfully loaded the virtual tree and validated the lockfile
      await time.start('npm-ci:rm', async () => {
        return await Promise.all([...workspacePaths.values()].map(async modulePath => {
          const fullPath = path.join(modulePath, 'node_modules')
          // get the list of entries so we can skip the glob for performance
          const entries = await fs.readdir(fullPath, null).catch(() => [])
          return Promise.all(entries.map(folder => {
            return fs.rm(path.join(fullPath, folder), { force: true, recursive: true })
          }))
        }))
      })
    }

    // Root lifecycle scripts for `npm ci` mirror those run by `npm install`. `preinstall` runs *before* reify so that scripts can bootstrap the environment (e.g. private-registry auth) before any dependency is fetched or unpacked. The remaining scripts run after reify as they did before.
    const scriptShell = this.npm.config.get('script-shell') || undefined
    const runRootScript = (event) => runScript({
      path: where,
      args: [],
      scriptShell,
      stdio: 'inherit',
      event,
    })

    if (!ignoreScripts) {
      await runRootScript('preinstall')
    }

    await arb.reify(opts)

    if (!ignoreScripts) {
      const postReifyScripts = [
        'install',
        'postinstall',
        'prepublish', // XXX should we remove this finally??
        'preprepare',
        'prepare',
        'postprepare',
      ]
      for (const event of postReifyScripts) {
        await runRootScript(event)
      }
    }
    await reifyFinish(this.npm, arb)
  }
}

module.exports = CI
