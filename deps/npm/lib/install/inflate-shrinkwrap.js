'use strict'

const BB = require('bluebird')

let addBundled
const childPath = require('../utils/child-path.js')
const createChild = require('./node.js').create
let fetchPackageMetadata
const inflateBundled = require('./inflate-bundled.js')
const moduleName = require('../utils/module-name.js')
const normalizePackageData = require('normalize-package-data')
const npm = require('../npm.js')
const realizeShrinkwrapSpecifier = require('./realize-shrinkwrap-specifier.js')
const validate = require('aproba')
const path = require('path')

module.exports = function (tree, sw, opts, finishInflating) {
  if (!fetchPackageMetadata) {
    fetchPackageMetadata = BB.promisify(require('../fetch-package-metadata.js'))
    addBundled = BB.promisify(fetchPackageMetadata.addBundled)
  }
  if (arguments.length === 3) {
    finishInflating = opts
    opts = {}
  }
  if (!npm.config.get('shrinkwrap') || !npm.config.get('package-lock')) {
    return finishInflating()
  }
  tree.loaded = false
  tree.hasRequiresFromLock = sw.requires
  return inflateShrinkwrap(tree.path, tree, sw.dependencies, opts).then(
    () => finishInflating(),
    finishInflating
  )
}

function inflateShrinkwrap (topPath, tree, swdeps, opts) {
  if (!swdeps) return Promise.resolve()
  if (!opts) opts = {}
  const onDisk = {}
  tree.children.forEach((child) => {
    onDisk[moduleName(child)] = child
  })

  tree.children = []

  return BB.each(Object.keys(swdeps), (name) => {
    const sw = swdeps[name]
    const dependencies = sw.dependencies || {}
    const requested = realizeShrinkwrapSpecifier(name, sw, topPath)
    return inflatableChild(
      onDisk[name], name, topPath, tree, sw, requested, opts
    ).then((child) => {
      child.hasRequiresFromLock = tree.hasRequiresFromLock
      return inflateShrinkwrap(topPath, child, dependencies)
    })
  })
}

function normalizePackageDataNoErrors (pkg) {
  try {
    normalizePackageData(pkg)
  } catch (ex) {
    // don't care
  }
}

function inflatableChild (onDiskChild, name, topPath, tree, sw, requested, opts) {
  validate('OSSOOOO|ZSSOOOO', arguments)
  if (onDiskChild && childIsEquivalent(sw, requested, onDiskChild)) {
    // The version on disk matches the shrinkwrap entry.
    if (!onDiskChild.fromShrinkwrap) onDiskChild.fromShrinkwrap = true
    onDiskChild.package._requested = requested
    onDiskChild.package._spec = requested.rawSpec
    onDiskChild.package._where = topPath
    onDiskChild.package._optional = sw.optional
    onDiskChild.package._development = sw.dev
    onDiskChild.package._inBundle = sw.bundled
    onDiskChild.fromBundle = (sw.bundled || onDiskChild.package._inBundle) ? tree.fromBundle || tree : null
    if (!onDiskChild.package._args) onDiskChild.package._args = []
    onDiskChild.package._args.push([String(requested), topPath])
    // non-npm registries can and will return unnormalized data, plus
    // even the npm registry may have package data normalized with older
    // normalization rules. This ensures we get package data in a consistent,
    // stable format.
    normalizePackageDataNoErrors(onDiskChild.package)
    onDiskChild.swRequires = sw.requires
    tree.children.push(onDiskChild)
    return BB.resolve(onDiskChild)
  } else if ((sw.version && sw.integrity) || sw.bundled) {
    // The shrinkwrap entry has an integrity field. We can fake a pkg to get
    // the installer to do a content-address fetch from the cache, if possible.
    return BB.resolve(makeFakeChild(name, topPath, tree, sw, requested))
  } else {
    // It's not on disk, and we can't just look it up by address -- do a full
    // fpm/inflate bundle pass. For registry deps, this will go straight to the
    // tarball URL, as if it were a remote tarball dep.
    return fetchChild(topPath, tree, sw, requested)
  }
}

function makeFakeChild (name, topPath, tree, sw, requested) {
  const from = sw.from || requested.raw
  const pkg = {
    name: name,
    version: sw.version,
    _id: name + '@' + sw.version,
    _resolved: adaptResolved(requested, sw.resolved),
    _requested: requested,
    _optional: sw.optional,
    _development: sw.dev,
    _inBundle: sw.bundled,
    _integrity: sw.integrity,
    _from: from,
    _spec: requested.rawSpec,
    _where: topPath,
    _args: [[requested.toString(), topPath]],
    dependencies: sw.requires
  }

  if (!sw.bundled) {
    const bundleDependencies = Object.keys(sw.dependencies || {}).filter((d) => sw.dependencies[d].bundled)
    if (bundleDependencies.length === 0) {
      pkg.bundleDependencies = bundleDependencies
    }
  }
  const child = createChild({
    package: pkg,
    loaded: true,
    parent: tree,
    children: [],
    fromShrinkwrap: true,
    fakeChild: sw,
    fromBundle: sw.bundled ? tree.fromBundle || tree : null,
    path: childPath(tree.path, pkg),
    realpath: childPath(tree.realpath, pkg),
    location: tree.location + '/' + pkg.name,
    isLink: requested.type === 'directory',
    isInLink: tree.isLink,
    swRequires: sw.requires
  })
  tree.children.push(child)
  return child
}

function adaptResolved (requested, resolved) {
  const registry = requested.scope
  ? npm.config.get(`${requested.scope}:registry`) || npm.config.get('registry')
  : npm.config.get('registry')
  if (!requested.registry || (resolved && resolved.indexOf(registry) === 0)) {
    // Nothing to worry about here. Pass it through.
    return resolved
  } else {
    // We could fast-path for registry.npmjs.org here, but if we do, it
    // would end up getting written back to the `resolved` field. By always
    // returning `null` for other registries, `pacote.extract()` will take
    // care of any required metadata fetches internally, without altering
    // the tree we're going to write out to shrinkwrap/lockfile.
    return null
  }
}

function fetchChild (topPath, tree, sw, requested) {
  return fetchPackageMetadata(requested, topPath).then((pkg) => {
    pkg._from = sw.from || requested.raw
    pkg._optional = sw.optional
    pkg._development = sw.dev
    pkg._inBundle = false
    return addBundled(pkg).then(() => pkg)
  }).then((pkg) => {
    var isLink = pkg._requested.type === 'directory'
    const child = createChild({
      package: pkg,
      loaded: false,
      parent: tree,
      fromShrinkwrap: requested,
      path: childPath(tree.path, pkg),
      realpath: isLink ? requested.fetchSpec : childPath(tree.realpath, pkg),
      children: pkg._bundled || [],
      location: tree.location + '/' + pkg.name,
      fromBundle: null,
      isLink: isLink,
      isInLink: tree.isLink,
      swRequires: sw.requires
    })
    tree.children.push(child)
    if (pkg._bundled) {
      delete pkg._bundled
      inflateBundled(child, child, child.children)
    }
    return child
  })
}

function childIsEquivalent (sw, requested, child) {
  if (!child) return false
  if (child.fromShrinkwrap) return true
  if (sw.integrity && child.package._integrity === sw.integrity) return true
  if (child.isLink && requested.type === 'directory') return path.relative(child.realpath, requested.fetchSpec) === ''

  if (sw.resolved) return child.package._resolved === sw.resolved
  if (!requested.registry && sw.from) return child.package._from === sw.from
  if (!requested.registry && child.package._resolved) return sw.version === child.package._resolved
  return child.package.version === sw.version
}
