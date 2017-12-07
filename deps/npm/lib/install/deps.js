'use strict'

const BB = require('bluebird')

var fs = require('fs')
var assert = require('assert')
var path = require('path')
var semver = require('semver')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var iferr = require('iferr')
var npa = require('npm-package-arg')
var validate = require('aproba')
var dezalgo = require('dezalgo')
var fetchPackageMetadata = require('../fetch-package-metadata.js')
var andAddParentToErrors = require('./and-add-parent-to-errors.js')
var addBundled = require('../fetch-package-metadata.js').addBundled
var readShrinkwrap = require('./read-shrinkwrap.js')
var inflateShrinkwrap = require('./inflate-shrinkwrap.js')
var inflateBundled = require('./inflate-bundled.js')
var andFinishTracker = require('./and-finish-tracker.js')
var npm = require('../npm.js')
var flatNameFromTree = require('./flatten-tree.js').flatNameFromTree
var createChild = require('./node.js').create
var resetMetadata = require('./node.js').reset
var isInstallable = require('./validate-args.js').isInstallable
var packageId = require('../utils/package-id.js')
var moduleName = require('../utils/module-name.js')
var isDevDep = require('./is-dev-dep.js')
var isProdDep = require('./is-prod-dep.js')
var reportOptionalFailure = require('./report-optional-failure.js')
var getSaveType = require('./save.js').getSaveType
var unixFormatPath = require('../utils/unix-format-path.js')
var isExtraneous = require('./is-extraneous.js')
var isRegistry = require('../utils/is-registry.js')

// The export functions in this module mutate a dependency tree, adding
// items to them.

var registryTypes = { range: true, version: true }

function doesChildVersionMatch (child, requested, requestor) {
  if (child.fromShrinkwrap && !child.hasRequiresFromLock) return true
  // ranges of * ALWAYS count as a match, because when downloading we allow
  // prereleases to match * if there are ONLY prereleases
  if (requested.type === 'range' && requested.fetchSpec === '*') return true

  if (requested.type === 'directory') {
    if (!child.isLink) return false
    return path.relative(child.realpath, requested.fetchSpec) === ''
  }

  if (!registryTypes[requested.type]) {
    var childReq = child.package._requested
    if (childReq) {
      if (childReq.rawSpec === requested.rawSpec) return true
      if (childReq.type === requested.type && childReq.saveSpec === requested.saveSpec) return true
    }
    // If _requested didn't exist OR if it didn't match then we'll try using
    // _from. We pass it through npa to normalize the specifier.
    // This can happen when installing from an `npm-shrinkwrap.json` where `_requested` will
    // be the tarball URL from `resolved` and thus can't match what's in the `package.json`.
    // In those cases _from, will be preserved and we can compare that to ensure that they
    // really came from the same sources.
    // You'll see this scenario happen with at least tags and git dependencies.
    // Some buggy clients will write spaces into the module name part of a _from.
    if (child.package._from) {
      var fromReq = npa.resolve(moduleName(child), child.package._from.replace(new RegExp('^\s*' + moduleName(child) + '\s*@'), ''))
      if (fromReq.rawSpec === requested.rawSpec) return true
      if (fromReq.type === requested.type && fromReq.saveSpec && fromReq.saveSpec === requested.saveSpec) return true
    }
    return false
  }
  try {
    return semver.satisfies(child.package.version, requested.fetchSpec, true)
  } catch (e) {
    return false
  }
}

function childDependencySpecifier (tree, name, spec) {
  return npa.resolve(name, spec, packageRelativePath(tree))
}

exports.computeMetadata = computeMetadata
function computeMetadata (tree, seen) {
  if (!seen) seen = new Set()
  if (!tree || seen.has(tree)) return
  seen.add(tree)
  if (tree.parent == null) {
    resetMetadata(tree)
    tree.isTop = true
  }
  tree.location = flatNameFromTree(tree)

  function findChild (name, spec, kind) {
    try {
      var req = childDependencySpecifier(tree, name, spec)
    } catch (err) {
      return
    }
    var child = findRequirement(tree, req.name, req)
    if (child) {
      resolveWithExistingModule(child, tree)
      return true
    }
    return
  }

  const deps = tree.package.dependencies || {}
  const reqs = tree.swRequires || {}
  for (let name of Object.keys(deps)) {
    if (findChild(name, deps[name])) continue
    if (findChild(name, reqs[name])) continue
    tree.missingDeps[name] = deps[name]
  }
  if (tree.isTop) {
    const devDeps = tree.package.devDependencies || {}
    for (let name of Object.keys(devDeps)) {
      if (findChild(name, devDeps[name])) continue
      tree.missingDevDeps[name] = devDeps[name]
    }
  }

  tree.children.filter((child) => !child.removed).forEach((child) => computeMetadata(child, seen))

  return tree
}

function isDep (tree, child) {
  var name = moduleName(child)
  var prodVer = isProdDep(tree, name)
  var devVer = isDevDep(tree, name)

  try {
    var prodSpec = childDependencySpecifier(tree, name, prodVer)
  } catch (err) {
    return {isDep: true, isProdDep: false, isDevDep: false}
  }
  var matches
  if (prodSpec) matches = doesChildVersionMatch(child, prodSpec, tree)
  if (matches) return {isDep: true, isProdDep: prodSpec, isDevDep: false}
  if (devVer === prodVer) return {isDep: child.fromShrinkwrap, isProdDep: false, isDevDep: false}
  try {
    var devSpec = childDependencySpecifier(tree, name, devVer)
    return {isDep: doesChildVersionMatch(child, devSpec, tree) || child.fromShrinkwrap, isProdDep: false, isDevDep: devSpec}
  } catch (err) {
    return {isDep: child.fromShrinkwrap, isProdDep: false, isDevDep: false}
  }
}

function addRequiredDep (tree, child) {
  var dep = isDep(tree, child)
  if (!dep.isDep) return false
  replaceModuleByPath(child, 'requiredBy', tree)
  replaceModuleByName(tree, 'requires', child)
  if (dep.isProdDep && tree.missingDeps) delete tree.missingDeps[moduleName(child)]
  if (dep.isDevDep && tree.missingDevDeps) delete tree.missingDevDeps[moduleName(child)]
  return true
}

exports.removeObsoleteDep = removeObsoleteDep
function removeObsoleteDep (child, log) {
  if (child.removed) return
  child.removed = true
  if (log) {
    log.silly('removeObsoleteDep', 'removing ' + packageId(child) +
      ' from the tree as its been replaced by a newer version or is no longer required')
  }
  // remove from physical tree
  if (child.parent) {
    child.parent.children = child.parent.children.filter(function (pchild) { return pchild !== child })
  }
  // remove from logical tree
  var requires = child.requires || []
  requires.forEach(function (requirement) {
    requirement.requiredBy = requirement.requiredBy.filter(function (reqBy) { return reqBy !== child })
    // we don't just check requirement.requires because that doesn't account
    // for circular deps.  isExtraneous does.
    if (isExtraneous(requirement)) removeObsoleteDep(requirement, log)
  })
}

function packageRelativePath (tree) {
  if (!tree) return ''
  var requested = tree.package._requested || {}
  var isLocal = requested.type === 'directory' || requested.type === 'file'
  return isLocal ? requested.fetchSpec
       : (tree.isLink || tree.isInLink) && !preserveSymlinks() ? tree.realpath
       : tree.path
}

function matchingDep (tree, name) {
  if (!tree || !tree.package) return
  if (tree.package.dependencies && tree.package.dependencies[name]) return tree.package.dependencies[name]
  if (tree.package.devDependencies && tree.package.devDependencies[name]) return tree.package.devDependencies[name]
  return
}

exports.getAllMetadata = function (args, tree, where, next) {
  asyncMap(args, function (arg, done) {
    let spec
    try {
      spec = npa(arg)
    } catch (e) {
      return done(e)
    }
    if (spec.type !== 'file' && spec.type !== 'directory' && (spec.name == null || spec.rawSpec === '')) {
      return fs.stat(path.join(arg, 'package.json'), (err) => {
        if (err) {
          var version = matchingDep(tree, spec.name)
          if (version) {
            try {
              return fetchPackageMetadata(npa.resolve(spec.name, version), where, done)
            } catch (e) {
              return done(e)
            }
          } else {
            return fetchPackageMetadata(spec, where, done)
          }
        } else {
          try {
            return fetchPackageMetadata(npa('file:' + arg), where, done)
          } catch (e) {
            return done(e)
          }
        }
      })
    } else {
      return fetchPackageMetadata(spec, where, done)
    }
  }, next)
}

// Add a list of args to tree's top level dependencies
exports.loadRequestedDeps = function (args, tree, saveToDependencies, log, next) {
  validate('AOOF', [args, tree, log, next])
  asyncMap(args, function (pkg, done) {
    var depLoaded = andAddParentToErrors(tree, done)
    resolveWithNewModule(pkg, tree, log.newGroup('loadRequestedDeps'), iferr(depLoaded, function (child, tracker) {
      validate('OO', arguments)
      if (npm.config.get('global')) {
        child.isGlobal = true
      }
      var childName = moduleName(child)
      child.saveSpec = computeVersionSpec(tree, child)
      child.userRequired = true
      child.save = getSaveType(tree, child)
      const types = ['dependencies', 'devDependencies', 'optionalDependencies']
      if (child.save) {
        tree.package[child.save][childName] = child.saveSpec
        // Astute readers might notice that this exact same code exists in
        // save.js under a different guise. That code is responsible for deps
        // being removed from the final written `package.json`. The removal in
        // this function is specifically to prevent "installed as both X and Y"
        // warnings when moving an existing dep between different dep fields.
        //
        // Or, try it by removing this loop, and do `npm i -P x && npm i -D x`
        for (let saveType of types) {
          if (child.save !== saveType) {
            delete tree.package[saveType][childName]
          }
        }
      }

      // For things the user asked to install, that aren't a dependency (or
      // won't be when we're done), flag it as "depending" on the user
      // themselves, so we don't remove it as a dep that no longer exists
      var childIsDep = addRequiredDep(tree, child)
      if (!childIsDep) child.userRequired = true
      depLoaded(null, child, tracker)
    }))
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

function isNotEmpty (value) {
  return value != null && value !== ''
}

exports.computeVersionSpec = computeVersionSpec
function computeVersionSpec (tree, child) {
  validate('OO', arguments)
  var requested
  var childReq = child.package._requested
  if (childReq && (isNotEmpty(childReq.saveSpec) || (isNotEmpty(childReq.rawSpec) && isNotEmpty(childReq.fetchSpec)))) {
    requested = child.package._requested
  } else if (child.package._from) {
    requested = npa(child.package._from)
  } else {
    requested = npa.resolve(child.package.name, child.package.version)
  }
  if (isRegistry(requested)) {
    var version = child.package.version
    var rangeDescriptor = ''
    if (semver.valid(version, true) &&
        semver.gte(version, '0.1.0', true) &&
        !npm.config.get('save-exact')) {
      rangeDescriptor = npm.config.get('save-prefix')
    }
    return rangeDescriptor + version
  } else if (requested.type === 'directory' || requested.type === 'file') {
    return 'file:' + unixFormatPath(path.relative(tree.path, requested.fetchSpec))
  } else {
    return requested.saveSpec || requested.rawSpec
  }
}

function moduleNameMatches (name) {
  return function (child) { return moduleName(child) === name }
}

// while this implementation does not require async calling, doing so
// gives this a consistent interface with loadDeps et al
exports.removeDeps = function (args, tree, saveToDependencies, next) {
  validate('AOSF|AOZF', [args, tree, saveToDependencies, next])
  for (let pkg of args) {
    var pkgName = moduleName(pkg)
    var toRemove = tree.children.filter(moduleNameMatches(pkgName))
    var pkgToRemove = toRemove[0] || createChild({package: {name: pkgName}})
    var saveType = getSaveType(tree, pkg) || 'dependencies'
    if (tree.isTop && saveToDependencies) {
      pkgToRemove.save = saveType
    }
    if (tree.package[saveType][pkgName]) {
      delete tree.package[saveType][pkgName]
      if (saveType === 'optionalDependencies' && tree.package.dependencies[pkgName]) {
        delete tree.package.dependencies[pkgName]
      }
    }
    replaceModuleByPath(tree, 'removedChildren', pkgToRemove)
    for (let parent of pkgToRemove.requiredBy) {
      parent.requires = parent.requires.filter((child) => child !== pkgToRemove)
    }
    pkgToRemove.requiredBy = pkgToRemove.requiredBy.filter((parent) => parent !== tree)
  }
  next()
}
exports.removeExtraneous = function (args, tree, next) {
  for (let pkg of args) {
    var pkgName = moduleName(pkg)
    var toRemove = tree.children.filter(moduleNameMatches(pkgName))
    if (toRemove.length) {
      removeObsoleteDep(toRemove[0])
    }
  }
  next()
}

function andForEachChild (load, next) {
  validate('F', [next])
  next = dezalgo(next)
  return function (er, children, logs) {
    // when children is empty, logs won't be passed in at all (asyncMap is weird)
    // so shortcircuit before arg validation
    if (!er && (!children || children.length === 0)) return next()
    validate('EAA', arguments)
    if (er) return next(er)
    assert(children.length === logs.length)
    var cmds = []
    for (var ii = 0; ii < children.length; ++ii) {
      cmds.push([load, children[ii], logs[ii]])
    }
    var sortedCmds = cmds.sort(function installOrder (aa, bb) {
      return moduleName(aa[1]).localeCompare(moduleName(bb[1]))
    })
    chain(sortedCmds, next)
  }
}

function isDepOptional (tree, name, pkg) {
  if (pkg.package && pkg.package._optional) return true
  if (!tree.package.optionalDependencies) return false
  if (tree.package.optionalDependencies[name] != null) return true
  return false
}

exports.failedDependency = failedDependency
function failedDependency (tree, name, pkg) {
  if (name) {
    if (isDepOptional(tree, name, pkg || {})) {
      return false
    }
  }

  tree.failed = true

  if (tree.isTop) return true

  if (tree.userRequired) return true

  if (!tree.requiredBy) return false

  let anyFailed = false
  for (var ii = 0; ii < tree.requiredBy.length; ++ii) {
    var requireParent = tree.requiredBy[ii]
    if (failedDependency(requireParent, moduleName(tree), tree)) {
      anyFailed = true
    }
  }
  return anyFailed
}

function andHandleOptionalErrors (log, tree, name, done) {
  validate('OOSF', arguments)
  return function (er, child, childLog) {
    if (!er) validate('OO', [child, childLog])
    if (!er) return done(er, child, childLog)
    var isFatal = failedDependency(tree, name)
    if (er && !isFatal) {
      reportOptionalFailure(tree, name, er)
      return done()
    } else {
      return done(er, child, childLog)
    }
  }
}

exports.prefetchDeps = prefetchDeps
function prefetchDeps (tree, deps, log, next) {
  validate('OOOF', arguments)
  var skipOptional = !npm.config.get('optional')
  var seen = new Set()
  const finished = andFinishTracker(log, next)
  const fpm = BB.promisify(fetchPackageMetadata)
  resolveBranchDeps(tree.package, deps).then(
    () => finished(), finished
  )

  function resolveBranchDeps (pkg, deps) {
    return BB.resolve(null).then(() => {
      var allDependencies = Object.keys(deps).map((dep) => {
        return npa.resolve(dep, deps[dep])
      }).filter((dep) => {
        return isRegistry(dep) &&
               !seen.has(dep.toString()) &&
               !findRequirement(tree, dep.name, dep)
      })
      if (skipOptional) {
        var optDeps = pkg.optionalDependencies || {}
        allDependencies = allDependencies.filter((dep) => !optDeps[dep.name])
      }
      return BB.map(allDependencies, (dep) => {
        seen.add(dep.toString())
        return fpm(dep, '', {tracker: log.newItem('fetchMetadata')}).then(
          (pkg) => {
            return pkg && pkg.dependencies && resolveBranchDeps(pkg, pkg.dependencies)
          },
          () => null
        )
      })
    })
  }
}

// Load any missing dependencies in the given tree
exports.loadDeps = loadDeps
function loadDeps (tree, log, next) {
  validate('OOF', arguments)
  if (tree.loaded || (tree.parent && tree.parent.failed) || tree.removed) return andFinishTracker.now(log, next)
  if (tree.parent) tree.loaded = true
  if (!tree.package.dependencies) tree.package.dependencies = {}
  asyncMap(Object.keys(tree.package.dependencies), function (dep, done) {
    var version = tree.package.dependencies[dep]
    if (tree.package.optionalDependencies &&
        tree.package.optionalDependencies[dep] &&
        !npm.config.get('optional')) {
      return done()
    }

    addDependency(dep, version, tree, log.newGroup('loadDep:' + dep), andHandleOptionalErrors(log, tree, dep, done))
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

// Load development dependencies into the given tree
exports.loadDevDeps = function (tree, log, next) {
  validate('OOF', arguments)
  if (!tree.package.devDependencies) return andFinishTracker.now(log, next)
  asyncMap(Object.keys(tree.package.devDependencies), function (dep, done) {
    // things defined as both dev dependencies and regular dependencies are treated
    // as the former
    if (tree.package.dependencies[dep]) return done()

    var logGroup = log.newGroup('loadDevDep:' + dep)
    addDependency(dep, tree.package.devDependencies[dep], tree, logGroup, done)
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

var loadExtraneous = exports.loadExtraneous = function (tree, log, next) {
  var seen = new Set()

  function loadExtraneous (tree) {
    if (seen.has(tree)) return
    seen.add(tree)
    for (var child of tree.children) {
      if (child.loaded) continue
      resolveWithExistingModule(child, tree)
      loadExtraneous(child)
    }
  }
  loadExtraneous(tree)
  log.finish()
  next()
}

exports.loadExtraneous.andResolveDeps = function (tree, log, next) {
  validate('OOF', arguments)
  // For canonicalized trees (eg from shrinkwrap) we don't want to bother
  // resolving the dependencies of extraneous deps.
  if (tree.loaded) return loadExtraneous(tree, log, next)
  asyncMap(tree.children.filter(function (child) { return !child.loaded }), function (child, done) {
    resolveWithExistingModule(child, tree)
    done(null, child, log)
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

function addDependency (name, versionSpec, tree, log, done) {
  validate('SSOOF', arguments)
  var next = andAddParentToErrors(tree, done)
  try {
    var req = childDependencySpecifier(tree, name, versionSpec)
    if (tree.swRequires && tree.swRequires[name]) {
      var swReq = childDependencySpecifier(tree, name, tree.swRequires[name])
    }
  } catch (err) {
    return done(err)
  }
  var child = findRequirement(tree, name, req)
  if (!child && swReq) child = findRequirement(tree, name, swReq)
  if (child) {
    resolveWithExistingModule(child, tree)
    if (child.package._shrinkwrap === undefined) {
      readShrinkwrap.andInflate(child, function (er) { next(er, child, log) })
    } else {
      next(null, child, log)
    }
  } else {
    fetchPackageMetadata(req, packageRelativePath(tree), {tracker: log.newItem('fetchMetadata')}, iferr(next, function (pkg) {
      resolveWithNewModule(pkg, tree, log, next)
    }))
  }
}

function resolveWithExistingModule (child, tree) {
  validate('OO', arguments)
  addRequiredDep(tree, child)
  if (tree.parent && child.parent !== tree) updatePhantomChildren(tree.parent, child)
}

var updatePhantomChildren = exports.updatePhantomChildren = function (current, child) {
  validate('OO', arguments)
  while (current && current !== child.parent) {
    if (!current.phantomChildren) current.phantomChildren = {}
    current.phantomChildren[moduleName(child)] = child
    current = current.parent
  }
}

exports._replaceModuleByPath = replaceModuleByPath
function replaceModuleByPath (obj, key, child) {
  return replaceModule(obj, key, child, function (replacing, child) {
    return replacing.path === child.path
  })
}

exports._replaceModuleByName = replaceModuleByName
function replaceModuleByName (obj, key, child) {
  var childName = moduleName(child)
  return replaceModule(obj, key, child, function (replacing, child) {
    return moduleName(replacing) === childName
  })
}

function replaceModule (obj, key, child, matchBy) {
  validate('OSOF', arguments)
  if (!obj[key]) obj[key] = []
  // we replace children with a new array object instead of mutating it
  // because mutating it results in weird failure states.
  // I would very much like to know _why_ this is. =/
  var children = [].concat(obj[key])
  for (var replaceAt = 0; replaceAt < children.length; ++replaceAt) {
    if (matchBy(children[replaceAt], child)) break
  }
  var replacing = children.splice(replaceAt, 1, child)
  obj[key] = children
  return replacing[0]
}

function resolveWithNewModule (pkg, tree, log, next) {
  validate('OOOF', arguments)

  log.silly('resolveWithNewModule', packageId(pkg), 'checking installable status')
  return isInstallable(pkg, (err) => {
    let installable = !err
    addBundled(pkg, (bundleErr) => {
      var parent = earliestInstallable(tree, tree, pkg) || tree
      var isLink = pkg._requested.type === 'directory'
      var child = createChild({
        package: pkg,
        parent: parent,
        path: path.join(parent.isLink ? parent.realpath : parent.path, 'node_modules', pkg.name),
        realpath: isLink ? pkg._requested.fetchSpec : path.join(parent.realpath, 'node_modules', pkg.name),
        children: pkg._bundled || [],
        isLink: isLink,
        isInLink: parent.isLink,
        knownInstallable: installable
      })
      if (!installable || bundleErr) child.failed = true
      delete pkg._bundled
      var hasBundled = child.children.length

      var replaced = replaceModuleByName(parent, 'children', child)
      if (replaced) removeObsoleteDep(replaced)
      addRequiredDep(tree, child)
      child.location = flatNameFromTree(child)

      if (tree.parent && parent !== tree) updatePhantomChildren(tree.parent, child)

      if (hasBundled) {
        inflateBundled(child, child, child.children)
      }

      if (pkg._shrinkwrap && pkg._shrinkwrap.dependencies) {
        return inflateShrinkwrap(child, pkg._shrinkwrap, (swErr) => {
          if (swErr) child.failed = true
          next(err || bundleErr || swErr, child, log)
        })
      }
      next(err || bundleErr, child, log)
    })
  })
}

var validatePeerDeps = exports.validatePeerDeps = function (tree, onInvalid) {
  if (!tree.package.peerDependencies) return
  Object.keys(tree.package.peerDependencies).forEach(function (pkgname) {
    var version = tree.package.peerDependencies[pkgname]
    try {
      var spec = npa.resolve(pkgname, version)
    } catch (e) {}
    var match = spec && findRequirement(tree.parent || tree, pkgname, spec)
    if (!match) onInvalid(tree, pkgname, version)
  })
}

exports.validateAllPeerDeps = function (tree, onInvalid) {
  validateAllPeerDeps(tree, onInvalid, new Set())
}

function validateAllPeerDeps (tree, onInvalid, seen) {
  validate('OFO', arguments)
  if (seen.has(tree)) return
  seen.add(tree)
  validatePeerDeps(tree, onInvalid)
  tree.children.forEach(function (child) { validateAllPeerDeps(child, onInvalid, seen) })
}

// Determine if a module requirement is already met by the tree at or above
// our current location in the tree.
var findRequirement = exports.findRequirement = function (tree, name, requested, requestor) {
  validate('OSO', [tree, name, requested])
  if (!requestor) requestor = tree
  var nameMatch = function (child) {
    return moduleName(child) === name && child.parent && !child.removed
  }
  var versionMatch = function (child) {
    return doesChildVersionMatch(child, requested, requestor)
  }
  if (nameMatch(tree)) {
    // this *is* the module, but it doesn't match the version, so a
    // new copy will have to be installed
    return versionMatch(tree) ? tree : null
  }

  var matches = tree.children.filter(nameMatch)
  if (matches.length) {
    matches = matches.filter(versionMatch)
    // the module exists as a dependent, but the version doesn't match, so
    // a new copy will have to be installed above here
    if (matches.length) return matches[0]
    return null
  }
  if (tree.isTop) return null
  if (!preserveSymlinks() && /^[.][.][\\/]/.test(path.relative(tree.parent.realpath, tree.realpath))) return null
  return findRequirement(tree.parent, name, requested, requestor)
}

function preserveSymlinks () {
  if (!('NODE_PRESERVE_SYMLINKS' in process.env)) return false
  const value = process.env.NODE_PRESERVE_SYMLINKS
  if (value == null || value === '' || value === 'false' || value === 'no' || value === '0') return false
  return true
}

// Find the highest level in the tree that we can install this module in.
// If the module isn't installed above us yet, that'd be the very top.
// If it is, then it's the level below where its installed.
var earliestInstallable = exports.earliestInstallable = function (requiredBy, tree, pkg) {
  validate('OOO', arguments)
  function undeletedModuleMatches (child) {
    return !child.removed && moduleName(child) === pkg.name
  }
  if (tree.children.some(undeletedModuleMatches)) return null

  // If any of the children of this tree have conflicting
  // binaries then we need to decline to install this package here.
  var binaryMatches = pkg.bin && tree.children.some(function (child) {
    if (child.removed || !child.package.bin) return false
    return Object.keys(child.package.bin).some(function (bin) {
      return pkg.bin[bin]
    })
  })

  if (binaryMatches) return null

  // if this tree location requested the same module then we KNOW it
  // isn't compatible because if it were findRequirement would have
  // found that version.
  var deps = tree.package.dependencies || {}
  if (!tree.removed && requiredBy !== tree && deps[pkg.name]) {
    return null
  }

  var devDeps = tree.package.devDependencies || {}
  if (tree.isTop && devDeps[pkg.name]) {
    var requested = childDependencySpecifier(tree, pkg.name, devDeps[pkg.name])
    if (!doesChildVersionMatch({package: pkg}, requested, tree)) {
      return null
    }
  }

  if (tree.phantomChildren && tree.phantomChildren[pkg.name]) return null

  if (tree.isTop) return tree
  if (tree.isGlobal) return tree

  if (npm.config.get('global-style') && tree.parent.isTop) return tree
  if (npm.config.get('legacy-bundling')) return tree

  if (!preserveSymlinks() && /^[.][.][\\/]/.test(path.relative(tree.parent.realpath, tree.realpath))) return tree

  return (earliestInstallable(requiredBy, tree.parent, pkg) || tree)
}
