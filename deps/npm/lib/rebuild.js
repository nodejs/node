const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const semver = require('semver')
const completion = require('./utils/completion/installed-deep.js')

const BaseCommand = require('./base-command.js')
class Rebuild extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Rebuild a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'rebuild'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<name>[@<version>] ...]']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  exec (args, cb) {
    this.rebuild(args).then(() => cb()).catch(cb)
  }

  async rebuild (args) {
    const globalTop = resolve(this.npm.globalDir, '..')
    const where = this.npm.config.get('global') ? globalTop : this.npm.prefix
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
    })

    if (args.length) {
      // get the set of nodes matching the name that we want rebuilt
      const tree = await arb.loadActual()
      const specs = args.map(arg => {
        const spec = npa(arg)
        if (spec.type === 'tag' && spec.rawSpec === '')
          return spec

        if (spec.type !== 'range' && spec.type !== 'version' && spec.type !== 'directory')
          throw new Error('`npm rebuild` only supports SemVer version/range specifiers')

        return spec
      })
      const nodes = tree.inventory.filter(node => this.isNode(specs, node))

      await arb.rebuild({ nodes })
    } else
      await arb.rebuild()

    this.npm.output('rebuilt dependencies successfully')
  }

  isNode (specs, node) {
    return specs.some(spec => {
      if (spec.type === 'directory')
        return node.path === spec.fetchSpec

      if (spec.name !== node.name)
        return false

      if (spec.rawSpec === '' || spec.rawSpec === '*')
        return true

      const { version } = node.package
      // TODO: add tests for a package with missing version
      return semver.satisfies(version, spec.fetchSpec)
    })
  }
}
module.exports = Rebuild
