const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const semver = require('semver')
const completion = require('../utils/completion/installed-deep.js')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
class Rebuild extends ArboristWorkspaceCmd {
  static description = 'Rebuild a package'
  static name = 'rebuild'
  static params = [
    'global',
    'bin-links',
    'foreground-scripts',
    'ignore-scripts',
    ...super.params,
  ]

  static usage = ['[<package-spec>] ...]']

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  async exec (args) {
    const globalTop = resolve(this.npm.globalDir, '..')
    const where = this.npm.global ? globalTop : this.npm.prefix
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
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

      await arb.rebuild({ nodes })
    } else {
      await arb.rebuild()
    }

    this.npm.output('rebuilt dependencies successfully')
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
