const { resolve } = require('path')

const semver = require('semver')
const libdiff = require('libnpmdiff')
const npa = require('npm-package-arg')
const Arborist = require('@npmcli/arborist')
const npmlog = require('npmlog')
const pacote = require('pacote')
const pickManifest = require('npm-pick-manifest')

const readLocalPkg = require('./utils/read-local-package.js')
const BaseCommand = require('./base-command.js')

class Diff extends BaseCommand {
  static get description () {
    return 'The registry diff command'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'diff'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      '[...<paths>]',
      '--diff=<pkg-name> [...<paths>]',
      '--diff=<version-a> [--diff=<version-b>] [...<paths>]',
      '--diff=<spec-a> [--diff=<spec-b>] [...<paths>]',
      '[--diff-ignore-all-space] [--diff-name-only] [...<paths>] [...<paths>]',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'diff',
      'diff-name-only',
      'diff-unified',
      'diff-ignore-all-space',
      'diff-no-prefix',
      'diff-src-prefix',
      'diff-dst-prefix',
      'diff-text',
      'global',
      'tag',
    ]
  }

  get where () {
    const globalTop = resolve(this.npm.globalDir, '..')
    const global = this.npm.config.get('global')
    return global ? globalTop : this.npm.prefix
  }

  exec (args, cb) {
    this.diff(args).then(() => cb()).catch(cb)
  }

  async diff (args) {
    const specs = this.npm.config.get('diff').filter(d => d)
    if (specs.length > 2) {
      throw new TypeError(
        'Can\'t use more than two --diff arguments.\n\n' +
        `Usage:\n${this.usage}`
      )
    }

    const [a, b] = await this.retrieveSpecs(specs)
    npmlog.info('diff', { src: a, dst: b })

    const res = await libdiff([a, b], {
      ...this.npm.flatOptions,
      diffFiles: args,
      where: this.where,
    })
    return this.npm.output(res)
  }

  async retrieveSpecs ([a, b]) {
    // no arguments, defaults to comparing cwd
    // to its latest published registry version
    if (!a)
      return this.defaultSpec()

    // single argument, used to compare wanted versions of an
    // installed dependency or to compare the cwd to a published version
    if (!b)
      return this.transformSingleSpec(a)

    const specs = await this.convertVersionsToSpecs([a, b])
    return this.findVersionsByPackageName(specs)
  }

  async defaultSpec () {
    let noPackageJson
    let pkgName
    try {
      pkgName = await readLocalPkg(this.npm)
    } catch (e) {
      npmlog.verbose('diff', 'could not read project dir package.json')
      noPackageJson = true
    }

    if (!pkgName || noPackageJson) {
      throw new Error(
        'Needs multiple arguments to compare or run from a project dir.\n\n' +
        `Usage:\n${this.usage}`
      )
    }

    return [
      `${pkgName}@${this.npm.config.get('tag')}`,
      `file:${this.npm.prefix}`,
    ]
  }

  async transformSingleSpec (a) {
    let noPackageJson
    let pkgName
    try {
      pkgName = await readLocalPkg(this.npm)
    } catch (e) {
      npmlog.verbose('diff', 'could not read project dir package.json')
      noPackageJson = true
    }
    const missingPackageJson = new Error(
      'Needs multiple arguments to compare or run from a project dir.\n\n' +
      `Usage:\n${this.usage}`
    )

    const specSelf = () => {
      if (noPackageJson)
        throw missingPackageJson

      return `file:${this.npm.prefix}`
    }

    // using a valid semver range, that means it should just diff
    // the cwd against a published version to the registry using the
    // same project name and the provided semver range
    if (semver.validRange(a)) {
      if (!pkgName)
        throw missingPackageJson

      return [
        `${pkgName}@${a}`,
        specSelf(),
      ]
    }

    // when using a single package name as arg and it's part of the current
    // install tree, then retrieve the current installed version and compare
    // it against the same value `npm outdated` would suggest you to update to
    const spec = npa(a)
    if (spec.registry) {
      let actualTree
      let node
      try {
        const opts = {
          ...this.npm.flatOptions,
          path: this.where,
        }
        const arb = new Arborist(opts)
        actualTree = await arb.loadActual(opts)
        node = actualTree &&
          actualTree.inventory.query('name', spec.name)
            .values().next().value
      } catch (e) {
        npmlog.verbose('diff', 'failed to load actual install tree')
      }

      if (!node || !node.name || !node.package || !node.package.version) {
        return [
          `${spec.name}@${spec.fetchSpec}`,
          specSelf(),
        ]
      }

      const tryRootNodeSpec = () =>
        (actualTree && actualTree.edgesOut.get(spec.name) || {}).spec

      const tryAnySpec = () => {
        for (const edge of node.edgesIn)
          return edge.spec
      }

      const aSpec = `file:${node.realpath}`

      // finds what version of the package to compare against, if a exact
      // version or tag was passed than it should use that, otherwise
      // work from the top of the arborist tree to find the original semver
      // range declared in the package that depends on the package.
      let bSpec
      if (spec.rawSpec)
        bSpec = spec.rawSpec
      else {
        const bTargetVersion =
          tryRootNodeSpec()
          || tryAnySpec()

        // figure out what to compare against,
        // follows same logic to npm outdated "Wanted" results
        const packument = await pacote.packument(spec, {
          ...this.npm.flatOptions,
          preferOnline: true,
        })
        bSpec = pickManifest(
          packument,
          bTargetVersion,
          { ...this.npm.flatOptions }
        ).version
      }

      return [
        `${spec.name}@${aSpec}`,
        `${spec.name}@${bSpec}`,
      ]
    } else if (spec.type === 'directory') {
      return [
        `file:${spec.fetchSpec}`,
        specSelf(),
      ]
    } else {
      throw new Error(
        'Spec type not supported.\n\n' +
        `Usage:\n${this.usage}`
      )
    }
  }

  async convertVersionsToSpecs ([a, b]) {
    const semverA = semver.validRange(a)
    const semverB = semver.validRange(b)

    // both specs are semver versions, assume current project dir name
    if (semverA && semverB) {
      let pkgName
      try {
        pkgName = await readLocalPkg(this.npm)
      } catch (e) {
        npmlog.verbose('diff', 'could not read project dir package.json')
      }

      if (!pkgName) {
        throw new Error(
          'Needs to be run from a project dir in order to diff two versions.\n\n' +
          `Usage:\n${this.usage}`
        )
      }
      return [`${pkgName}@${a}`, `${pkgName}@${b}`]
    }

    // otherwise uses the name from the other arg to
    // figure out the spec.name of what to compare
    if (!semverA && semverB)
      return [a, `${npa(a).name}@${b}`]

    if (semverA && !semverB)
      return [`${npa(b).name}@${a}`, b]

    // no valid semver ranges used
    return [a, b]
  }

  async findVersionsByPackageName (specs) {
    let actualTree
    try {
      const opts = {
        ...this.npm.flatOptions,
        path: this.where,
      }
      const arb = new Arborist(opts)
      actualTree = await arb.loadActual(opts)
    } catch (e) {
      npmlog.verbose('diff', 'failed to load actual install tree')
    }

    return specs.map(i => {
      const spec = npa(i)
      if (spec.rawSpec)
        return i

      const node = actualTree
        && actualTree.inventory.query('name', spec.name)
          .values().next().value

      const res = !node || !node.package || !node.package.version
        ? spec.fetchSpec
        : `file:${node.realpath}`

      return `${spec.name}@${res}`
    })
  }
}

module.exports = Diff
