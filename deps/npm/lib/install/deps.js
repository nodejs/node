'use strict'
var assert = require('assert')
var path = require('path')
var semver = require('semver')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var union = require('lodash.union')
var iferr = require('iferr')
var npa = require('npm-package-arg')
var validate = require('aproba')
var realizePackageSpecifier = require('realize-package-specifier')
var realizeShrinkwrapSpecifier = require('./realize-shrinkwrap-specifier')
var asap = require('asap')
var dezalgo = require('dezalgo')
var fetchPackageMetadata = require('../fetch-package-metadata.js')
var andAddParentToErrors = require('./and-add-parent-to-errors.js')
var addShrinkwrap = require('../fetch-package-metadata.js').addShrinkwrap
var addBundled = require('../fetch-package-metadata.js').addBundled
var readShrinkwrap = require('./read-shrinkwrap.js')
var inflateShrinkwrap = require('./inflate-shrinkwrap.js')
var inflateBundled = require('./inflate-bundled.js')
var andFinishTracker = require('./and-finish-tracker.js')
var npm = require('../npm.js')
var flatNameFromTree = require('./flatten-tree.js').flatNameFromTree
var createChild = require('./node.js').create
var resetMetadata = require('./node.js').reset
var andIgnoreErrors = require('./and-ignore-errors.js')
var isInstallable = require('./validate-args.js').isInstallable
var packageId = require('../utils/package-id.js')
var moduleName = require('../utils/module-name.js')
var isDevDep = require('./is-dev-dep.js')
var isProdDep = require('./is-prod-dep.js')
var reportOptionalFailure = require('./report-optional-failure.js')

// The export functions in this module mutate a dependency tree, adding
// items to them.

function isDep (tree, child, cb) {
  var name = moduleName(child)
  var prodVer = isProdDep(tree, name)
  var devVer = isDevDep(tree, name)

  childDependencySpecifier(tree, name, prodVer, function (er, prodSpec) {
    if (er) return cb(child.fromShrinkwrap)
    var matches
    if (prodSpec) matches = doesChildVersionMatch(child, prodSpec, tree)
    if (matches) return cb(true, prodSpec)
    if (devVer === prodVer) return cb(child.fromShrinkwrap)
    childDependencySpecifier(tree, name, devVer, function (er, devSpec) {
      if (er) return cb(child.fromShrinkwrap)
      cb(doesChildVersionMatch(child, devSpec, tree) || child.fromShrinkwrap, null, devSpec)
    })
  })
}

var registryTypes = { range: true, version: true }

function doesChildVersionMatch (child, requested, requestor) {
  // we always consider deps provided by a shrinkwrap as "correct" or else
  // we'll subvert them if they're intentionally "invalid"
  if (child.parent === requestor && child.fromShrinkwrap) return true
  // ranges of * ALWAYS count as a match, because when downloading we allow
  // prereleases to match * if there are ONLY prereleases
  if (requested.spec === '*') return true

  var childReq = child.package._requested
  if (!childReq) childReq = npa(moduleName(child) + '@' + child.package._from)
  if (childReq) {
    if (childReq.rawSpec === requested.rawSpec) return true
    if (childReq.type === requested.type && childReq.spec === requested.spec) return true
  }
  // If _requested didn't exist OR if it didn't match then we'll try using
  // _from. We pass it through npa to normalize the specifier.
  // This can happen when installing from an `npm-shrinkwrap.json` where `_requested` will
  // be the tarball URL from `resolved` and thus can't match what's in the `package.json`.
  // In those cases _from, will be preserved and we can compare that to ensure that they
  // really came from the same sources.
  // You'll see this scenario happen with at least tags and git dependencies.
  if (!registryTypes[requested.type]) {
    if (child.package._from) {
      var fromReq = npa(child.package._from)
      if (fromReq.rawSpec === requested.rawSpec) return true
      if (fromReq.type === requested.type && fromReq.spec === requested.spec) return true
    }
    return false
  }
  return semver.satisfies(child.package.version, requested.spec)
}

// TODO: Rename to maybe computeMetadata or computeRelationships
exports.recalculateMetadata = function (tree, log, next) {
  recalculateMetadata(tree, log, {}, next)
}

exports._childDependencySpecifier = childDependencySpecifier
function childDependencySpecifier (tree, name, spec, cb) {
  if (!tree.resolved) tree.resolved = {}
  if (!tree.resolved[name]) tree.resolved[name] = {}
  if (tree.resolved[name][spec]) {
    return asap(function () {
      cb(null, tree.resolved[name][spec])
    })
  }
  realizePackageSpecifier(name + '@' + spec, packageRelativePath(tree), function (er, req) {
    if (er) return cb(er)
    tree.resolved[name][spec] = req
    cb(null, req)
  })
}

function recalculateMetadata (tree, log, seen, next) {
  validate('OOOF', arguments)
  if (seen[tree.path]) return next()
  seen[tree.path] = true
  if (tree.parent == null) {
    resetMetadata(tree)
    tree.isTop = true
  }

  function markDeps (toMark, done) {
    var name = toMark.name
    var spec = toMark.spec
    var kind = toMark.kind
    childDependencySpecifier(tree, name, spec, function (er, req) {
      if (er || !req.name) return done()
      var child = findRequirement(tree, req.name, req)
      if (child) {
        resolveWithExistingModule(child, tree, log, andIgnoreErrors(done))
      } else if (kind === 'dep') {
        tree.missingDeps[req.name] = req.rawSpec
        done()
      } else if (kind === 'dev') {
        tree.missingDevDeps[req.name] = req.rawSpec
        done()
      } else {
        done()
      }
    })
  }

  function makeMarkable (deps, kind) {
    if (!deps) return []
    return Object.keys(deps).map(function (depname) { return { name: depname, spec: deps[depname], kind: kind } })
  }

  // Ensure dependencies and dev dependencies are marked as required
  var tomark = makeMarkable(tree.package.dependencies, 'dep')
  if (tree.isTop) tomark = union(tomark, makeMarkable(tree.package.devDependencies, 'dev'))

  // Ensure any children ONLY from a shrinkwrap are also included
  var childrenOnlyInShrinkwrap = tree.children.filter(function (child) {
    return child.fromShrinkwrap &&
      !tree.package.dependencies[child.package.name] &&
      !tree.package.devDependencies[child.package.name]
  })
  var tomarkOnlyInShrinkwrap = childrenOnlyInShrinkwrap.map(function (child) {
    var name = child.package.name
    var matched = child.package._spec.match(/^@?[^@]+@(.*)$/)
    var spec = matched ? matched[1] : child.package._spec
    var kind = tree.package.dependencies[name] ? 'dep'
             : tree.package.devDependencies[name] ? 'dev'
             : 'dep'
    return { name: name, spec: spec, kind: kind }
  })
  tomark = union(tomark, tomarkOnlyInShrinkwrap)

  // Don't bother trying to recalc children of failed deps
  tree.children = tree.children.filter(function (child) { return !child.failed })

  chain([
    [asyncMap, tomark, markDeps],
    [asyncMap, tree.children, function (child, done) { recalculateMetadata(child, log, seen, done) }]
  ], function () {
    tree.location = flatNameFromTree(tree)
    next(null, tree)
  })
}

function addRequiredDep (tree, child, cb) {
  isDep(tree, child, function (childIsDep, childIsProdDep, childIsDevDep) {
    if (!childIsDep) return cb(false)
    replaceModuleByPath(child, 'requiredBy', tree)
    replaceModuleByName(tree, 'requires', child)
    if (childIsProdDep && tree.missingDeps) delete tree.missingDeps[moduleName(child)]
    if (childIsDevDep && tree.missingDevDeps) delete tree.missingDevDeps[moduleName(child)]
    cb(true)
  })
}

exports.removeObsoleteDep = removeObsoleteDep
function removeObsoleteDep (child) {
  if (child.removed) return
  child.removed = true
  // remove from physical tree
  if (child.parent) {
    child.parent.children = child.parent.children.filter(function (pchild) { return pchild !== child })
  }
  // remove from logical tree
  var requires = child.requires || []
  requires.forEach(function (requirement) {
    requirement.requiredBy = requirement.requiredBy.filter(function (reqBy) { return reqBy !== child })
    if (requirement.requiredBy.length === 0) removeObsoleteDep(requirement)
  })
}

function matchingDep (tree, name) {
  if (tree.package.dependencies && tree.package.dependencies[name]) return tree.package.dependencies[name]
  if (tree.package.devDependencies && tree.package.devDependencies[name]) return tree.package.devDependencies[name]
  return
}

function packageRelativePath (tree) {
  if (!tree) return ''
  var requested = tree.package._requested || {}
  var isLocal = requested.type === 'directory' || requested.type === 'local'
  return isLocal ? requested.spec : tree.path
}

function getShrinkwrap (tree, name) {
  return tree.package._shrinkwrap && tree.package._shrinkwrap.dependencies && tree.package._shrinkwrap.dependencies[name]
}

exports.getAllMetadata = function (args, tree, where, next) {
  asyncMap(args, function (arg, done) {
    function fetchMetadataWithVersion () {
      var version = matchingDep(tree, arg)
      var spec = version == null ? arg : arg + '@' + version
      return fetchPackageMetadata(spec, where, done)
    }
    if (tree && arg.lastIndexOf('@') <= 0) {
      var sw = getShrinkwrap(tree, arg)
      if (sw) {
        return realizeShrinkwrapSpecifier(arg, sw, where, function (err, spec) {
          if (err) {
            return fetchMetadataWithVersion()
          } else {
            return fetchPackageMetadata(spec, where, done)
          }
        })
      } else {
        return fetchMetadataWithVersion()
      }
    } else {
      return fetchPackageMetadata(arg, where, done)
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
      if (saveToDependencies) {
        tree.package[saveToDependencies][childName] =
          child.package._requested.rawSpec || child.package._requested.spec
      }
      if (saveToDependencies && saveToDependencies !== 'devDependencies') {
        tree.package.dependencies[childName] =
          child.package._requested.rawSpec || child.package._requested.spec
      }
      child.userRequired = true
      child.save = saveToDependencies

      // For things the user asked to install, that aren't a dependency (or
      // won't be when we're done), flag it as "depending" on the user
      // themselves, so we don't remove it as a dep that no longer exists
      addRequiredDep(tree, child, function (childIsDep) {
        if (!childIsDep) child.userRequired = true
        depLoaded(null, child, tracker)
      })
    }))
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

function moduleNameMatches (name) {
  return function (child) { return moduleName(child) === name }
}

function noModuleNameMatches (name) {
  return function (child) { return moduleName(child) !== name }
}

// while this implementation does not require async calling, doing so
// gives this a consistent interface with loadDeps et al
exports.removeDeps = function (args, tree, saveToDependencies, log, next) {
  validate('AOOF', [args, tree, log, next])
  args.forEach(function (pkg) {
    var pkgName = moduleName(pkg)
    var toRemove = tree.children.filter(moduleNameMatches(pkgName))
    var pkgToRemove = toRemove[0] || createChild({package: {name: pkgName}})
    if (saveToDependencies) {
      replaceModuleByPath(tree, 'removed', pkgToRemove)
      pkgToRemove.save = saveToDependencies
    }
    removeObsoleteDep(pkgToRemove)
  })
  log.finish()
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

var failedDependency = exports.failedDependency = function (tree, name_pkg) {
  var name
  var pkg = {}
  if (typeof name_pkg === 'string') {
    name = name_pkg
  } else {
    pkg = name_pkg
    name = moduleName(pkg)
  }
  tree.children = tree.children.filter(noModuleNameMatches(name))

  if (isDepOptional(tree, name, pkg)) {
    return false
  }

  tree.failed = true

  if (tree.isTop) return true

  if (tree.userRequired) return true

  removeObsoleteDep(tree)

  if (!tree.requiredBy) return false

  for (var ii = 0; ii < tree.requiredBy.length; ++ii) {
    var requireParent = tree.requiredBy[ii]
    if (failedDependency(requireParent, tree.package)) {
      return true
    }
  }
  return false
}

function andHandleOptionalErrors (log, tree, name, done) {
  validate('OOSF', arguments)
  return function (er, child, childLog) {
    if (!er) validate('OO', [child, childLog])
    if (!er) return done(er, child, childLog)
    var isFatal = failedDependency(tree, name)
    if (er && !isFatal) {
      tree.children = tree.children.filter(noModuleNameMatches(name))
      reportOptionalFailure(tree, name, er)
      return done()
    } else {
      return done(er, child, childLog)
    }
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
  // if any of our prexisting children are from a shrinkwrap then we skip
  // loading dev deps as the shrinkwrap will already have provided them for us.
  if (tree.children.some(function (child) { return child.shrinkwrapDev })) {
    return andFinishTracker.now(log, next)
  }
  asyncMap(Object.keys(tree.package.devDependencies), function (dep, done) {
    // things defined as both dev dependencies and regular dependencies are treated
    // as the former
    if (tree.package.dependencies[dep]) return done()

    var logGroup = log.newGroup('loadDevDep:' + dep)
    addDependency(dep, tree.package.devDependencies[dep], tree, logGroup, done)
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

var loadExtraneous = exports.loadExtraneous = function (tree, log, next) {
  var seen = {}
  function loadExtraneous (tree, log, next) {
    validate('OOF', arguments)
    if (seen[tree.path]) return next()
    seen[tree.path] = true
    asyncMap(tree.children.filter(function (child) { return !child.loaded }), function (child, done) {
      resolveWithExistingModule(child, tree, log, done)
    }, andForEachChild(loadExtraneous, andFinishTracker(log, next)))
  }
  loadExtraneous(tree, log, next)
}

exports.loadExtraneous.andResolveDeps = function (tree, log, next) {
  validate('OOF', arguments)
  // For canonicalized trees (eg from shrinkwrap) we don't want to bother
  // resolving the dependencies of extraneous deps.
  if (tree.loaded) return loadExtraneous(tree, log, next)
  asyncMap(tree.children.filter(function (child) { return !child.loaded }), function (child, done) {
    resolveWithExistingModule(child, tree, log, done)
  }, andForEachChild(loadDeps, andFinishTracker(log, next)))
}

function addDependency (name, versionSpec, tree, log, done) {
  validate('SSOOF', arguments)
  var next = andAddParentToErrors(tree, done)
  childDependencySpecifier(tree, name, versionSpec, iferr(done, function (req) {
    var child = findRequirement(tree, name, req)
    if (child) {
      resolveWithExistingModule(child, tree, log, iferr(next, function (child, log) {
        if (child.package._shrinkwrap === undefined) {
          readShrinkwrap.andInflate(child, function (er) { next(er, child, log) })
        } else {
          next(null, child, log)
        }
      }))
    } else {
      fetchPackageMetadata(req, packageRelativePath(tree), log.newItem('fetchMetadata'), iferr(next, function (pkg) {
        resolveWithNewModule(pkg, tree, log, next)
      }))
    }
  }))
}

function resolveWithExistingModule (child, tree, log, next) {
  validate('OOOF', arguments)
  addRequiredDep(tree, child, function () {
    if (tree.parent && child.parent !== tree) updatePhantomChildren(tree.parent, child)
    next(null, child, log)
  })
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
  return isInstallable(pkg, iferr(next, function () {
    if (!pkg._from) {
      pkg._from = pkg._requested.name + '@' + pkg._requested.spec
    }
    addShrinkwrap(pkg, iferr(next, function () {
      addBundled(pkg, iferr(next, function () {
        var parent = earliestInstallable(tree, tree, pkg) || tree
        var child = createChild({
          package: pkg,
          parent: parent,
          path: path.join(parent.path, 'node_modules', pkg.name),
          realpath: path.resolve(parent.realpath, 'node_modules', pkg.name),
          children: pkg._bundled || [],
          isLink: tree.isLink,
          knownInstallable: true
        })
        delete pkg._bundled
        var hasBundled = child.children.length

        var replaced = replaceModuleByName(parent, 'children', child)
        if (replaced) removeObsoleteDep(replaced)
        addRequiredDep(tree, child, function () {
          child.location = flatNameFromTree(child)

          if (tree.parent && parent !== tree) updatePhantomChildren(tree.parent, child)

          if (hasBundled) {
            inflateBundled(child, child.children)
          }

          if (pkg._shrinkwrap && pkg._shrinkwrap.dependencies) {
            return inflateShrinkwrap(child, pkg._shrinkwrap.dependencies, function (er) {
              next(er, child, log)
            })
          }

          next(null, child, log)
        })
      }))
    }))
  }))
}

var validatePeerDeps = exports.validatePeerDeps = function (tree, onInvalid) {
  if (!tree.package.peerDependencies) return
  Object.keys(tree.package.peerDependencies).forEach(function (pkgname) {
    var version = tree.package.peerDependencies[pkgname]
    var match = findRequirement(tree.parent || tree, pkgname, npa(pkgname + '@' + version))
    if (!match) onInvalid(tree, pkgname, version)
  })
}

exports.validateAllPeerDeps = function (tree, onInvalid) {
  validateAllPeerDeps(tree, onInvalid, {})
}

function validateAllPeerDeps (tree, onInvalid, seen) {
  validate('OFO', arguments)
  if (seen[tree.path]) return
  seen[tree.path] = true
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
  return findRequirement(tree.parent, name, requested, requestor)
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

  if (tree.phantomChildren && tree.phantomChildren[pkg.name]) return null

  if (tree.isTop) return tree
  if (tree.isGlobal) return tree

  if (npm.config.get('global-style') && tree.parent.isTop) return tree
  if (npm.config.get('legacy-bundling')) return tree

  return (earliestInstallable(requiredBy, tree.parent, pkg) || tree)
}
