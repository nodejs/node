'use strict'

const BB = require('bluebird')

const chain = require('slide').chain
const detectIndent = require('detect-indent')
const fs = BB.promisifyAll(require('graceful-fs'))
const getRequested = require('./install/get-requested.js')
const id = require('./install/deps.js')
const iferr = require('iferr')
const isDevDep = require('./install/is-dev-dep.js')
const isOptDep = require('./install/is-opt-dep.js')
const isProdDep = require('./install/is-prod-dep.js')
const lifecycle = require('./utils/lifecycle.js')
const log = require('npmlog')
const moduleName = require('./utils/module-name.js')
const move = require('move-concurrently')
const npm = require('./npm.js')
const path = require('path')
const readPackageTree = BB.promisify(require('read-package-tree'))
const ssri = require('ssri')
const validate = require('aproba')
const writeFileAtomic = require('write-file-atomic')

const PKGLOCK = 'package-lock.json'
const SHRINKWRAP = 'npm-shrinkwrap.json'
const PKGLOCK_VERSION = npm.lockfileVersion

// emit JSON describing versions of all packages currently installed (for later
// use with shrinkwrap install)
shrinkwrap.usage = 'npm shrinkwrap'

module.exports = exports = shrinkwrap
function shrinkwrap (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (args.length) {
    log.warn('shrinkwrap', "doesn't take positional args")
  }

  move(
    path.resolve(npm.prefix, PKGLOCK),
    path.resolve(npm.prefix, SHRINKWRAP),
    { Promise: BB }
  ).then(() => {
    log.notice('', `${PKGLOCK} has been renamed to ${SHRINKWRAP}. ${SHRINKWRAP} will be used for future installations.`)
    return fs.readFileAsync(path.resolve(npm.prefix, SHRINKWRAP)).then((d) => {
      return JSON.parse(d)
    })
  }, (err) => {
    if (err.code !== 'ENOENT') {
      throw err
    } else {
      return readPackageTree(npm.localPrefix).then(
        id.computeMetadata
      ).then((tree) => {
        return BB.fromNode((cb) => {
          createShrinkwrap(tree, {
            silent,
            defaultFile: SHRINKWRAP
          }, cb)
        })
      })
    }
  }).then((data) => cb(null, data), cb)
}

module.exports.createShrinkwrap = createShrinkwrap

function createShrinkwrap (tree, opts, cb) {
  opts = opts || {}
  lifecycle(tree.package, 'preshrinkwrap', tree.path, function () {
    const pkginfo = treeToShrinkwrap(tree)
    chain([
      [lifecycle, tree.package, 'shrinkwrap', tree.path],
      [shrinkwrap_, tree.path, pkginfo, opts],
      [lifecycle, tree.package, 'postshrinkwrap', tree.path]
    ], iferr(cb, function (data) {
      cb(null, pkginfo)
    }))
  })
}

function treeToShrinkwrap (tree) {
  validate('O', arguments)
  var pkginfo = {}
  if (tree.package.name) pkginfo.name = tree.package.name
  if (tree.package.version) pkginfo.version = tree.package.version
  if (tree.children.length) {
    shrinkwrapDeps(pkginfo.dependencies = {}, tree, tree)
  }
  return pkginfo
}

function shrinkwrapDeps (deps, top, tree, seen) {
  validate('OOO', [deps, top, tree])
  if (!seen) seen = {}
  if (seen[tree.path]) return
  seen[tree.path] = true
  tree.children.sort(function (aa, bb) { return moduleName(aa).localeCompare(moduleName(bb)) }).forEach(function (child) {
    var childIsOnlyDev = isOnlyDev(child)
    if (child.fakeChild) {
      deps[moduleName(child)] = child.fakeChild
      return
    }
    var pkginfo = deps[moduleName(child)] = {}
    var req = child.package._requested || getRequested(child)
    if (req.type === 'directory' || req.type === 'file') {
      pkginfo.version = 'file:' + path.relative(top.path, child.package._resolved || req.fetchSpec)
    } else if (!req.registry && !child.fromBundle) {
      pkginfo.version = child.package._resolved || req.saveSpec || req.rawSpec
    } else {
      pkginfo.version = child.package.version
    }
    if (child.fromBundle || child.isInLink) {
      pkginfo.bundled = true
    } else {
      if (req.registry) {
        pkginfo.resolved = child.package._resolved
      }
      // no integrity for git deps as integirty hashes are based on the
      // tarball and we can't (yet) create consistent tarballs from a stable
      // source.
      if (req.type !== 'git') {
        pkginfo.integrity = child.package._integrity
        if (!pkginfo.integrity && child.package._shasum) {
          pkginfo.integrity = ssri.fromHex(child.package._shasum, 'sha1')
        }
      }
    }
    if (childIsOnlyDev) pkginfo.dev = true
    if (isOptional(child)) pkginfo.optional = true
    if (child.children.length) {
      pkginfo.dependencies = {}
      shrinkwrapDeps(pkginfo.dependencies, top, child, seen)
    }
  })
}

function shrinkwrap_ (dir, pkginfo, opts, cb) {
  save(dir, pkginfo, opts, cb)
}

function save (dir, pkginfo, opts, cb) {
  // copy the keys over in a well defined order
  // because javascript objects serialize arbitrarily
  BB.join(
    checkPackageFile(dir, SHRINKWRAP),
    checkPackageFile(dir, PKGLOCK),
    checkPackageFile(dir, 'package.json'),
    (shrinkwrap, lockfile, pkg) => {
      const info = (
        shrinkwrap ||
        lockfile ||
        {
          path: path.resolve(dir, opts.defaultFile || PKGLOCK),
          data: '{}',
          indent: (pkg && pkg.indent) || 2
        }
      )
      const updated = updateLockfileMetadata(pkginfo, pkg && pkg.data)
      const swdata = JSON.stringify(updated, null, info.indent) + '\n'
      writeFileAtomic(info.path, swdata, (err) => {
        if (err) return cb(err)
        if (opts.silent) return cb(null, pkginfo)
        if (!shrinkwrap && !lockfile) {
          log.notice('', `created a lockfile as ${path.basename(info.path)}. You should commit this file.`)
        }
        cb(null, pkginfo)
      })
    }
  ).then((file) => {
  }, cb)
}

function updateLockfileMetadata (pkginfo, pkgJson) {
  // This is a lot of work just to make sure the extra metadata fields are
  // between version and dependencies fields, without affecting any other stuff
  const newPkg = {}
  let metainfoWritten = false
  const metainfo = new Set([
    'lockfileVersion',
    'preserveSymlinks'
  ])
  Object.keys(pkginfo).forEach((k) => {
    if (k === 'dependencies') {
      writeMetainfo(newPkg)
    }
    if (!metainfo.has(k)) {
      newPkg[k] = pkginfo[k]
    }
    if (k === 'version') {
      writeMetainfo(newPkg)
    }
  })
  if (!metainfoWritten) {
    writeMetainfo(newPkg)
  }
  function writeMetainfo (pkginfo) {
    pkginfo.lockfileVersion = PKGLOCK_VERSION
    if (process.env.NODE_PRESERVE_SYMLINKS) {
      pkginfo.preserveSymlinks = process.env.NODE_PRESERVE_SYMLINKS
    }
    metainfoWritten = true
  }
  return newPkg
}

function checkPackageFile (dir, name) {
  const file = path.resolve(dir, name)
  return fs.readFileAsync(
    file, 'utf8'
  ).then((data) => {
    return {
      path: file,
      data: JSON.parse(data),
      indent: detectIndent(data).indent || 2
    }
  }).catch({code: 'ENOENT'}, () => {})
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
// ultimately return true.
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
