'use strict'

const BB = require('bluebird')

const chain = require('slide').chain
const detectIndent = require('detect-indent')
const detectNewline = require('detect-newline')
const readFile = BB.promisify(require('graceful-fs').readFile)
const getRequested = require('./install/get-requested.js')
const id = require('./install/deps.js')
const iferr = require('iferr')
const isOnlyOptional = require('./install/is-only-optional.js')
const isOnlyDev = require('./install/is-only-dev.js')
const lifecycle = require('./utils/lifecycle.js')
const log = require('npmlog')
const moduleName = require('./utils/module-name.js')
const move = require('move-concurrently')
const npm = require('./npm.js')
const path = require('path')
const readPackageTree = BB.promisify(require('read-package-tree'))
const ssri = require('ssri')
const stringifyPackage = require('stringify-package')
const validate = require('aproba')
const writeFileAtomic = require('write-file-atomic')
const unixFormatPath = require('./utils/unix-format-path.js')
const isRegistry = require('./utils/is-registry.js')

const PKGLOCK = 'package-lock.json'
const SHRINKWRAP = 'npm-shrinkwrap.json'
const PKGLOCK_VERSION = npm.lockfileVersion

// emit JSON describing versions of all packages currently installed (for later
// use with shrinkwrap install)
shrinkwrap.usage = 'npm shrinkwrap'

module.exports = exports = shrinkwrap
exports.treeToShrinkwrap = treeToShrinkwrap

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
    return readFile(path.resolve(npm.prefix, SHRINKWRAP)).then((d) => {
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
    pkginfo.requires = true
    shrinkwrapDeps(pkginfo.dependencies = {}, tree, tree)
  }
  return pkginfo
}

function shrinkwrapDeps (deps, top, tree, seen) {
  validate('OOO', [deps, top, tree])
  if (!seen) seen = new Set()
  if (seen.has(tree)) return
  seen.add(tree)
  sortModules(tree.children).forEach(function (child) {
    var childIsOnlyDev = isOnlyDev(child)
    var pkginfo = deps[moduleName(child)] = {}
    var requested = getRequested(child) || child.package._requested || {}
    var linked = child.isLink || child.isInLink
    var linkedFromHere = linked && path.relative(top.realpath, child.realpath)[0] !== '.'
    pkginfo.version = childVersion(top, child, requested)
    if (requested.type === 'git' && child.package._from) {
      pkginfo.from = child.package._from
    }
    if (child.fromBundle && !linked) {
      pkginfo.bundled = true
    } else {
      if (isRegistry(requested)) {
        pkginfo.resolved = child.package._resolved
      }
      // no integrity for git deps as integirty hashes are based on the
      // tarball and we can't (yet) create consistent tarballs from a stable
      // source.
      if (requested.type !== 'git') {
        pkginfo.integrity = child.package._integrity || undefined
        if (!pkginfo.integrity && child.package._shasum) {
          pkginfo.integrity = ssri.fromHex(child.package._shasum, 'sha1')
        }
      }
    }
    if (childIsOnlyDev) pkginfo.dev = true
    if (isOnlyOptional(child)) pkginfo.optional = true
    if (child.requires.length) {
      pkginfo.requires = {}
      sortModules(child.requires).forEach((required) => {
        var requested = getRequested(required, child) || required.package._requested || {}
        pkginfo.requires[moduleName(required)] = childRequested(top, required, requested)
      })
    }
    // iterate into children on non-links and links contained within the top level package
    if (child.children.length && (!child.isLink || linkedFromHere)) {
      pkginfo.dependencies = {}
      shrinkwrapDeps(pkginfo.dependencies, top, child, seen)
    }
  })
}

function sortModules (modules) {
  // sort modules with the locale-agnostic Unicode sort
  var sortedModuleNames = modules.map(moduleName).sort()
  return modules.sort((a, b) => (
    sortedModuleNames.indexOf(moduleName(a)) - sortedModuleNames.indexOf(moduleName(b))
  ))
}

function childVersion (top, child, req) {
  if (req.type === 'directory' || req.type === 'file') {
    return 'file:' + unixFormatPath(path.relative(top.path, child.package._resolved || req.fetchSpec))
  } else if (!isRegistry(req) && !child.fromBundle) {
    return child.package._resolved || req.saveSpec || req.rawSpec
  } else {
    return child.package.version
  }
}

function childRequested (top, child, requested) {
  if (requested.type === 'directory' || requested.type === 'file') {
    return 'file:' + unixFormatPath(path.relative(top.path, child.package._resolved || requested.fetchSpec))
  } else if (requested.type === 'git' && child.package._from) {
    return child.package._from
  } else if (!isRegistry(requested) && !child.fromBundle) {
    return child.package._resolved || requested.saveSpec || requested.rawSpec
  } else if (requested.type === 'tag') {
    // tags are not ranges we can match against, so we invent a "reasonable"
    // one based on what we actually installed.
    return npm.config.get('save-prefix') + child.package.version
  } else if (requested.saveSpec || requested.rawSpec) {
    return requested.saveSpec || requested.rawSpec
  } else if (child.package._from || (child.package._requested && child.package._requested.rawSpec)) {
    return child.package._from.replace(/^@?[^@]+@/, '') || child.package._requested.rawSpec
  } else {
    return child.package.version
  }
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
          indent: pkg && pkg.indent,
          newline: pkg && pkg.newline
        }
      )
      const updated = updateLockfileMetadata(pkginfo, pkg && JSON.parse(pkg.raw))
      const swdata = stringifyPackage(updated, info.indent, info.newline)
      if (swdata === info.raw) {
        // skip writing if file is identical
        log.verbose('shrinkwrap', `skipping write for ${path.basename(info.path)} because there were no changes.`)
        cb(null, pkginfo)
      } else {
        writeFileAtomic(info.path, swdata, (err) => {
          if (err) return cb(err)
          if (opts.silent) return cb(null, pkginfo)
          if (!shrinkwrap && !lockfile) {
            log.notice('', `created a lockfile as ${path.basename(info.path)}. You should commit this file.`)
          }
          cb(null, pkginfo)
        })
      }
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
  return readFile(
    file, 'utf8'
  ).then((data) => {
    return {
      path: file,
      raw: data,
      indent: detectIndent(data).indent,
      newline: detectNewline(data)
    }
  }).catch({code: 'ENOENT'}, () => {})
}
