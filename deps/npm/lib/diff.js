const { resolve } = require('path')

const semver = require('semver')
const libdiff = require('libnpmdiff')
const npa = require('npm-package-arg')
const Arborist = require('@npmcli/arborist')
const npmlog = require('npmlog')
const pacote = require('pacote')
const pickManifest = require('npm-pick-manifest')

const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const output = require('./utils/output.js')
const readLocalPkg = require('./utils/read-local-package.js')

const usage = usageUtil(
  'diff',
  'npm diff [...<paths>]' +
  '\nnpm diff --diff=<pkg-name> [...<paths>]' +
  '\nnpm diff --diff=<version-a> [--diff=<version-b>] [...<paths>]' +
  '\nnpm diff --diff=<spec-a> [--diff=<spec-b>] [...<paths>]' +
  '\nnpm diff [--diff-ignore-all-space] [--diff-name-only] [...<paths>] [...<paths>]'
)

const cmd = (args, cb) => diff(args).then(() => cb()).catch(cb)

const where = () => {
  const globalTop = resolve(npm.globalDir, '..')
  const { global } = npm.flatOptions
  return global ? globalTop : npm.prefix
}

const diff = async (args) => {
  const specs = npm.flatOptions.diff.filter(d => d)
  if (specs.length > 2) {
    throw new TypeError(
      'Can\'t use more than two --diff arguments.\n\n' +
      `Usage:\n${usage}`
    )
  }

  const [a, b] = await retrieveSpecs(specs)
  npmlog.info('diff', { src: a, dst: b })

  const res = await libdiff([a, b], { ...npm.flatOptions, diffFiles: args })
  return output(res)
}

const retrieveSpecs = ([a, b]) => {
  // no arguments, defaults to comparing cwd
  // to its latest published registry version
  if (!a)
    return defaultSpec()

  // single argument, used to compare wanted versions of an
  // installed dependency or to compare the cwd to a published version
  if (!b)
    return transformSingleSpec(a)

  return convertVersionsToSpecs([a, b])
    .then(findVersionsByPackageName)
}

const defaultSpec = async () => {
  let noPackageJson
  let pkgName
  try {
    pkgName = await readLocalPkg()
  } catch (e) {
    npmlog.verbose('diff', 'could not read project dir package.json')
    noPackageJson = true
  }

  if (!pkgName || noPackageJson) {
    throw new Error(
      'Needs multiple arguments to compare or run from a project dir.\n\n' +
      `Usage:\n${usage}`
    )
  }

  return [
    `${pkgName}@${npm.flatOptions.defaultTag}`,
    `file:${npm.prefix}`,
  ]
}

const transformSingleSpec = async (a) => {
  let noPackageJson
  let pkgName
  try {
    pkgName = await readLocalPkg()
  } catch (e) {
    npmlog.verbose('diff', 'could not read project dir package.json')
    noPackageJson = true
  }
  const missingPackageJson = new Error(
    'Needs multiple arguments to compare or run from a project dir.\n\n' +
    `Usage:\n${usage}`
  )

  const specSelf = () => {
    if (noPackageJson)
      throw missingPackageJson

    return `file:${npm.prefix}`
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
        ...npm.flatOptions,
        path: where(),
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
        ...npm.flatOptions,
        preferOnline: true,
      })
      bSpec = pickManifest(
        packument,
        bTargetVersion,
        { ...npm.flatOptions }
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
      `Usage:\n${usage}`
    )
  }
}

const convertVersionsToSpecs = async ([a, b]) => {
  const semverA = semver.validRange(a)
  const semverB = semver.validRange(b)

  // both specs are semver versions, assume current project dir name
  if (semverA && semverB) {
    let pkgName
    try {
      pkgName = await readLocalPkg()
    } catch (e) {
      npmlog.verbose('diff', 'could not read project dir package.json')
    }

    if (!pkgName) {
      throw new Error(
        'Needs to be run from a project dir in order to diff two versions.\n\n' +
        `Usage:\n${usage}`
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

const findVersionsByPackageName = async (specs) => {
  let actualTree
  try {
    const opts = {
      ...npm.flatOptions,
      path: where(),
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

module.exports = Object.assign(cmd, { usage })
