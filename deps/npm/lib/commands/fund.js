const archy = require('archy')
const Arborist = require('@npmcli/arborist')
const chalk = require('chalk')
const pacote = require('pacote')
const semver = require('semver')
const npa = require('npm-package-arg')
const { depth } = require('treeverse')
const {
  readTree: getFundingInfo,
  normalizeFunding,
  isValidFunding,
} = require('libnpmfund')

const completion = require('../utils/completion/installed-deep.js')
const openUrl = require('../utils/open-url.js')
const ArboristWorkspaceCmd = require('../arborist-cmd.js')

const getPrintableName = ({ name, version }) => {
  const printableVersion = version ? `@${version}` : ''
  return `${name}${printableVersion}`
}

class Fund extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Retrieve funding information'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'fund'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'json',
      'browser',
      'unicode',
      'workspace',
      'which',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg>]']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  async exec (args) {
    const spec = args[0]
    const numberArg = this.npm.config.get('which')

    const fundingSourceNumber = numberArg && parseInt(numberArg, 10)

    const badFundingSourceNumber =
      numberArg !== null &&
      (String(fundingSourceNumber) !== numberArg || fundingSourceNumber < 1)

    if (badFundingSourceNumber) {
      const err = new Error('`npm fund [<@scope>/]<pkg> [--which=fundingSourceNumber]` must be given a positive integer')
      err.code = 'EFUNDNUMBER'
      throw err
    }

    if (this.npm.config.get('global')) {
      const err = new Error('`npm fund` does not support global packages')
      err.code = 'EFUNDGLOBAL'
      throw err
    }

    const where = this.npm.prefix
    const arb = new Arborist({ ...this.npm.flatOptions, path: where })
    const tree = await arb.loadActual()

    if (spec) {
      await this.openFundingUrl({
        path: where,
        tree,
        spec,
        fundingSourceNumber,
      })
      return
    }

    // TODO: add !workspacesEnabled option handling to libnpmfund
    const fundingInfo = getFundingInfo(tree, {
      ...this.flatOptions,
      log: this.npm.log,
      workspaces: this.workspaceNames,
    })

    if (this.npm.config.get('json'))
      this.npm.output(this.printJSON(fundingInfo))
    else
      this.npm.output(this.printHuman(fundingInfo))
  }

  printJSON (fundingInfo) {
    return JSON.stringify(fundingInfo, null, 2)
  }

  printHuman (fundingInfo) {
    const color = this.npm.color
    const unicode = this.npm.config.get('unicode')
    const seenUrls = new Map()

    const tree = obj =>
      archy(obj, '', { unicode })

    const result = depth({
      tree: fundingInfo,

      // composes human readable package name
      // and creates a new archy item for readable output
      visit: ({ name, version, funding }) => {
        const [fundingSource] = []
          .concat(normalizeFunding(funding))
          .filter(isValidFunding)
        const { url } = fundingSource || {}
        const pkgRef = getPrintableName({ name, version })
        let item = {
          label: pkgRef,
        }

        if (url) {
          item.label = tree({
            label: color ? chalk.bgBlack.white(url) : url,
            nodes: [pkgRef],
          }).trim()

          // stacks all packages together under the same item
          if (seenUrls.has(url)) {
            item = seenUrls.get(url)
            item.label += `, ${pkgRef}`
            return null
          } else
            seenUrls.set(url, item)
        }

        return item
      },

      // puts child nodes back into returned archy
      // output while also filtering out missing items
      leave: (item, children) => {
        if (item)
          item.nodes = children.filter(Boolean)

        return item
      },

      // turns tree-like object return by libnpmfund
      // into children to be properly read by treeverse
      getChildren: (node) =>
        Object.keys(node.dependencies || {})
          .map(key => ({
            name: key,
            ...node.dependencies[key],
          })),
    })

    const res = tree(result)
    return color ? chalk.reset(res) : res
  }

  async openFundingUrl ({ path, tree, spec, fundingSourceNumber }) {
    const arg = npa(spec, path)
    const retrievePackageMetadata = () => {
      if (arg.type === 'directory') {
        if (tree.path === arg.fetchSpec) {
          // matches cwd, e.g: npm fund .
          return tree.package
        } else {
          // matches any file path within current arborist inventory
          for (const item of tree.inventory.values()) {
            if (item.path === arg.fetchSpec)
              return item.package
          }
        }
      } else {
        // tries to retrieve a package from arborist inventory
        // by matching resulted package name from the provided spec
        const [item] = [...tree.inventory.query('name', arg.name)]
          .filter(i => semver.valid(i.package.version))
          .sort((a, b) => semver.rcompare(a.package.version, b.package.version))

        if (item)
          return item.package
      }
    }

    const { funding } = retrievePackageMetadata() ||
      await pacote.manifest(arg, this.npm.flatOptions).catch(() => ({}))

    const validSources = []
      .concat(normalizeFunding(funding))
      .filter(isValidFunding)

    const matchesValidSource =
      validSources.length === 1 ||
      (fundingSourceNumber > 0 && fundingSourceNumber <= validSources.length)

    if (matchesValidSource) {
      const index = fundingSourceNumber ? fundingSourceNumber - 1 : 0
      const { type, url } = validSources[index]
      const typePrefix = type ? `${type} funding` : 'Funding'
      const msg = `${typePrefix} available at the following URL`
      return openUrl(this.npm, url, msg)
    } else if (validSources.length && !(fundingSourceNumber >= 1)) {
      validSources.forEach(({ type, url }, i) => {
        const typePrefix = type ? `${type} funding` : 'Funding'
        const msg = `${typePrefix} available at the following URL`
        this.npm.output(`${i + 1}: ${msg}: ${url}`)
      })
      this.npm.output('Run `npm fund [<@scope>/]<pkg> --which=1`, for example, to open the first funding URL listed in that package')
    } else {
      const noFundingError = new Error(`No valid funding method available for: ${spec}`)
      noFundingError.code = 'ENOFUND'

      throw noFundingError
    }
  }
}
module.exports = Fund
