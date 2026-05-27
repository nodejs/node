const { resolve } = require('node:path')
const { log, output } = require('proc-log')
const npa = require('npm-package-arg')
const semver = require('semver')
const ArboristWorkspaceCmd = require('../arborist-cmd.js')
const checkAllowScripts = require('../utils/check-allow-scripts.js')
const resolveAllowScripts = require('../utils/resolve-allow-scripts.js')
const strictAllowScriptsPreflight = require('../utils/strict-allow-scripts-preflight.js')

class Rebuild extends ArboristWorkspaceCmd {
  static description = 'Rebuild a package'
  static name = 'rebuild'
  static params = [
    'global',
    'bin-links',
    'foreground-scripts',
    'ignore-scripts',
    'allow-scripts',
    'strict-allow-scripts',
    'dangerously-allow-all-scripts',
    ...super.params,
  ]

  static usage = ['[<package-spec>] ...]']

  static async completion (opts, npm) {
    const completion = require('../utils/installed-deep.js')
    return completion(npm, opts)
  }

  async exec (args) {
    const globalTop = resolve(this.npm.globalDir, '..')
    const where = this.npm.global ? globalTop : this.npm.prefix
    const Arborist = require('@npmcli/arborist')
    const { policy: allowScriptsPolicy } = await resolveAllowScripts(this.npm)
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
      allowScripts: allowScriptsPolicy,
      // TODO when extending ReifyCmd
      // workspaces: this.workspaceNames,
    })

    if (args.length) {
      // get the set of nodes matching the name that we want rebuilt
      const tree = await arb.loadActual()
      const specs = args.map(arg => {
        const spec = npa(arg)
        if (spec.rawSpec === '*') {
          return spec
        }

        if (spec.type !== 'range' && spec.type !== 'version' && spec.type !== 'directory') {
          throw new Error('`npm rebuild` only supports SemVer version/range specifiers')
        }

        return spec
      })
      const nodes = tree.inventory.filter(node => this.isNode(specs, node))

      await strictAllowScriptsPreflight({ arb, npm: this.npm })
      await arb.rebuild({ nodes })
    } else {
      await arb.loadActual()
      await strictAllowScriptsPreflight({ arb, npm: this.npm })
      await arb.rebuild()
    }

    // Phase 1 advisory: list any packages whose install scripts ran (or
    // would have run) and are not yet covered by allowScripts. Rebuild
    // doesn't go through reifyFinish, so the walker is invoked here.
    const unreviewed = await checkAllowScripts({ arb, npm: this.npm })
    if (unreviewed.length > 0) {
      const count = unreviewed.length
      const noun = count === 1 ? 'package has' : 'packages have'
      log.warn(
        'rebuild',
        `${count} ${noun} install scripts not yet covered by allowScripts. ` +
        'Run `npm approve-scripts --allow-scripts-pending` to review.'
      )
    }

    output.standard('rebuilt dependencies successfully')
  }

  isNode (specs, node) {
    return specs.some(spec => {
      if (spec.type === 'directory') {
        return node.path === spec.fetchSpec
      }

      if (spec.name !== node.name) {
        return false
      }

      if (spec.rawSpec === '' || spec.rawSpec === '*') {
        return true
      }

      const { version } = node.package
      // TODO: add tests for a package with missing version
      return semver.satisfies(version, spec.fetchSpec)
    })
  }
}

module.exports = Rebuild
