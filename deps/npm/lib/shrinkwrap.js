// emit JSON describing versions of all packages currently installed (for later
// use with shrinkwrap install)

module.exports = exports = shrinkwrap

var path = require('path')
var log = require('npmlog')
var writeFileAtomic = require('write-file-atomic')
var iferr = require('iferr')
var readPackageJson = require('read-package-json')
var readPackageTree = require('read-package-tree')
var validate = require('aproba')
var chain = require('slide').chain
var npm = require('./npm.js')
var recalculateMetadata = require('./install/deps.js').recalculateMetadata
var validatePeerDeps = require('./install/deps.js').validatePeerDeps
var isExtraneous = require('./install/is-extraneous.js')
var packageId = require('./utils/package-id.js')
var moduleName = require('./utils/module-name.js')
var output = require('./utils/output.js')
var lifecycle = require('./utils/lifecycle.js')
var isDevDep = require('./install/is-dev-dep.js')
var isProdDep = require('./install/is-prod-dep.js')
var isOptDep = require('./install/is-opt-dep.js')

shrinkwrap.usage = 'npm shrinkwrap'

function shrinkwrap (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (args.length) {
    log.warn('shrinkwrap', "doesn't take positional args")
  }

  var packagePath = path.join(npm.localPrefix, 'package.json')
  var dev = !!npm.config.get('dev') || /^dev(elopment)?$/.test(npm.config.get('also'))

  readPackageJson(packagePath, iferr(cb, function (pkg) {
    createShrinkwrap(npm.localPrefix, pkg, dev, silent, cb)
  }))
}

module.exports.createShrinkwrap = createShrinkwrap

function createShrinkwrap (dir, pkg, dev, silent, cb) {
  lifecycle(pkg, 'preshrinkwrap', dir, function () {
    readPackageTree(dir, andRecalculateMetadata(iferr(cb, function (tree) {
      var pkginfo = treeToShrinkwrap(tree, dev)

      chain([
        [lifecycle, tree.package, 'shrinkwrap', dir],
        [shrinkwrap_, pkginfo, silent],
        [lifecycle, tree.package, 'postshrinkwrap', dir]
      ], iferr(cb, function (data) {
        cb(null, data[0])
      }))
    })))
  })
}

function andRecalculateMetadata (next) {
  validate('F', arguments)
  return function (er, tree) {
    validate('EO', arguments)
    if (er) return next(er)
    recalculateMetadata(tree, log, next)
  }
}

function treeToShrinkwrap (tree, dev) {
  validate('OB', arguments)
  var pkginfo = {}
  if (tree.package.name) pkginfo.name = tree.package.name
  if (tree.package.version) pkginfo.version = tree.package.version
  var problems = []
  if (tree.children.length) {
    shrinkwrapDeps(dev, problems, pkginfo.dependencies = {}, tree)
  }
  if (problems.length) pkginfo.problems = problems
  return pkginfo
}

function shrinkwrapDeps (dev, problems, deps, tree, seen) {
  validate('BAOO', [dev, problems, deps, tree])
  if (!seen) seen = {}
  if (seen[tree.path]) return
  seen[tree.path] = true
  Object.keys(tree.missingDeps).forEach(function (name) {
    var invalid = tree.children.filter(function (dep) { return moduleName(dep) === name })[0]
    if (invalid) {
      problems.push('invalid: have ' + invalid.package._id + ' (expected: ' + tree.missingDeps[name] + ') ' + invalid.path)
    } else if (!tree.package.optionalDependencies || !tree.package.optionalDependencies[name]) {
      var topname = packageId(tree)
      problems.push('missing: ' + name + '@' + tree.package.dependencies[name] +
        (topname ? ', required by ' + topname : ''))
    }
  })
  tree.children.sort(function (aa, bb) { return moduleName(aa).localeCompare(moduleName(bb)) }).forEach(function (child) {
    var childIsOnlyDev = isOnlyDev(child)
    if (!dev && childIsOnlyDev) {
      log.warn('shrinkwrap', 'Excluding devDependency: %s', child.location)
      return
    }
    var pkginfo = deps[moduleName(child)] = {}
    pkginfo.version = child.package.version
    pkginfo.from = child.package._from
    pkginfo.resolved = child.package._resolved
    if (dev && childIsOnlyDev) pkginfo.dev = true
    if (isOptional(child)) pkginfo.optional = true
    if (isExtraneous(child)) {
      problems.push('extraneous: ' + child.package._id + ' ' + child.path)
    }
    validatePeerDeps(child, function (tree, pkgname, version) {
      problems.push('peer invalid: ' + pkgname + '@' + version +
        ', required by ' + child.package._id)
    })
    if (child.children.length) {
      shrinkwrapDeps(dev, problems, pkginfo.dependencies = {}, child, seen)
    }
  })
}

function shrinkwrap_ (pkginfo, silent, cb) {
  if (pkginfo.problems) {
    return cb(new Error('Problems were encountered\n' +
                        'Please correct and try again.\n' +
                        pkginfo.problems.join('\n')))
  }

  save(pkginfo, silent, cb)
}

function save (pkginfo, silent, cb) {
  // copy the keys over in a well defined order
  // because javascript objects serialize arbitrarily
  var swdata
  try {
    swdata = JSON.stringify(pkginfo, null, 2) + '\n'
  } catch (er) {
    log.error('shrinkwrap', 'Error converting package info to json')
    return cb(er)
  }

  var file = path.resolve(npm.prefix, 'npm-shrinkwrap.json')

  writeFileAtomic(file, swdata, function (er) {
    if (er) return cb(er)
    if (silent) return cb(null, pkginfo)
    output('wrote npm-shrinkwrap.json')
    cb(null, pkginfo)
  })
}

// Returns true if the module `node` is only required direcctly as a dev
// dependency of the top level or transitively _from_ top level dev
// dependencies.
// Dual mode modules (that are both dev AND prod) should return false.
function isOnlyDev (node, seen) {
  if (!seen) seen = {}
  return node.requiredBy.length && node.requiredBy.every(andIsOnlyDev(moduleName(node), seen))
}

// There is a known limitation with this implementation: If a dependency is
// ONLY required by cycles that are detached from the top level then it will
// ultimately return ture.
//
// This is ok though: We don't allow shrinkwraps with extraneous deps and
// these situation is caught by the extraneous checker before we get here.
function andIsOnlyDev (name, seen) {
  return function (req) {
    var isDev = isDevDep(req, name)
    var isProd = isProdDep(req, name)
    if (req.isTop) {
      return isDev && !isProd
    } else {
      if (seen[req.path]) return true
      seen[req.path] = true
      return isOnlyDev(req, seen)
    }
  }
}

function isOptional (node, seen) {
  if (!seen) seen = {}
  // If a node is not required by anything, then we've reached
  // the top level package.
  if (seen[node.path] || node.requiredBy.length === 0) {
    return false
  }
  seen[node.path] = true

  return node.requiredBy.every(function (req) {
    return isOptDep(req, node.package.name) || isOptional(req, seen)
  })
}
