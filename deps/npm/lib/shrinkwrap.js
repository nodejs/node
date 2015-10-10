// emit JSON describing versions of all packages currently installed (for later
// use with shrinkwrap install)

module.exports = exports = shrinkwrap

var path = require('path')
var log = require('npmlog')
var writeFileAtomic = require('write-file-atomic')
var iferr = require('iferr')
var readPackageTree = require('read-package-tree')
var validate = require('aproba')
var npm = require('./npm.js')
var recalculateMetadata = require('./install/deps.js').recalculateMetadata
var validatePeerDeps = require('./install/deps.js').validatePeerDeps
var isExtraneous = require('./install/is-extraneous.js')
var isOnlyDev = require('./install/is-dev.js').isOnlyDev
var getPackageId = require('./install/get-package-id.js')

shrinkwrap.usage = 'npm shrinkwrap'

function shrinkwrap (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (args.length) {
    log.warn('shrinkwrap', "doesn't take positional args")
  }

  var dir = path.resolve(npm.dir, '..')
  npm.config.set('production', true)
  readPackageTree(dir, andRecalculateMetadata(iferr(cb, function (tree) {
    var pkginfo = treeToShrinkwrap(tree, !!npm.config.get('dev') || /^dev(elopment)?$/.test(npm.config.get('also')))
    shrinkwrap_(pkginfo, silent, cb)
  })))
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
    var invalid = tree.children.filter(function (dep) { return dep.package.name === name })[0]
    if (invalid) {
      problems.push('invalid: have ' + invalid.package._id + ' (expected: ' + tree.missingDeps[name] + ') ' + invalid.path)
    } else {
      var topname = getPackageId(tree)
      problems.push('missing: ' + name + '@' + tree.package.dependencies[name] +
        (topname ? ', required by ' + topname : ''))
    }
  })
  tree.children.sort(function (aa, bb) { return aa.package.name.localeCompare(bb.package.name) }).forEach(function (child) {
    if (!dev && isOnlyDev(child)) {
      log.warn('shrinkwrap', 'Excluding devDependency: %s', child.package.name, child.parent.package.dependencies)
      return
    }
    var pkginfo = deps[child.package.name] = {}
    pkginfo.version = child.package.version
    pkginfo.from = child.package._from
    pkginfo.resolved = child.package._resolved
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
    log.clearProgress()
    console.log('wrote npm-shrinkwrap.json')
    log.showProgress()
    cb(null, pkginfo)
  })
}
