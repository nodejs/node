// commands for packing and unpacking tarballs
// this file is used by lib/cache.js

var fs = require('graceful-fs')
var path = require('path')
var writeFileAtomic = require('write-file-atomic')
var writeStreamAtomic = require('fs-write-stream-atomic')
var log = require('npmlog')
var uidNumber = require('uid-number')
var readJson = require('read-package-json')
var tar = require('tar')
var zlib = require('zlib')
var fstream = require('fstream')
var Packer = require('fstream-npm')
var iferr = require('iferr')
var inherits = require('inherits')
var npm = require('../npm.js')
var rm = require('./gently-rm.js')
var myUid = process.getuid && process.getuid()
var myGid = process.getgid && process.getgid()
var readPackageTree = require('read-package-tree')
var union = require('lodash.union')
var flattenTree = require('../install/flatten-tree.js')
var moduleName = require('./module-name.js')
var packageId = require('./package-id.js')

if (process.env.SUDO_UID && myUid === 0) {
  if (!isNaN(process.env.SUDO_UID)) myUid = +process.env.SUDO_UID
  if (!isNaN(process.env.SUDO_GID)) myGid = +process.env.SUDO_GID
}

exports.pack = pack
exports.unpack = unpack

function pack (tarball, folder, pkg, cb) {
  log.verbose('tar pack', [tarball, folder])

  log.verbose('tarball', tarball)
  log.verbose('folder', folder)

  readJson(path.join(folder, 'package.json'), function (er, pkg) {
    if (er || !pkg.bundleDependencies) {
      pack_(tarball, folder, null, null, pkg, cb)
    } else {
      // we require this at runtime due to load-order issues, because recursive
      // requires fail if you replace the exports object, and we do, not in deps, but
      // in a dep of it.
      var recalculateMetadata = require('../install/deps.js').recalculateMetadata

      readPackageTree(folder, iferr(cb, function (tree) {
        recalculateMetadata(tree, log.newGroup('pack:' + packageId(pkg)), iferr(cb, function () {
          pack_(tarball, folder, tree, flattenTree(tree), pkg, cb)
        }))
      }))
    }
  })
}

function BundledPacker (props) {
  Packer.call(this, props)
}
inherits(BundledPacker, Packer)

function nameMatch (name) { return function (other) { return name === moduleName(other) } }

function pack_ (tarball, folder, tree, flatTree, pkg, cb) {
  function InstancePacker (props) {
    BundledPacker.call(this, props)
  }
  inherits(InstancePacker, BundledPacker)
  InstancePacker.prototype.isBundled = function (name) {
    var bd = this.package && this.package.bundleDependencies
    if (!bd) return false

    if (!Array.isArray(bd)) {
      throw new Error(packageId(this) + '\'s `bundledDependencies` should ' +
                      'be an array')
    }
    if (!tree) return false

    if (bd.indexOf(name) !== -1) return true
    var pkg = tree.children.filter(nameMatch(name))[0]
    if (!pkg) return false
    var requiredBy = union([], pkg.package._requiredBy)
    var seen = {}
    while (requiredBy.length) {
      var req = requiredBy.shift()
      if (seen[req]) continue
      seen[req] = true
      var reqPkg = flatTree[req]
      if (!reqPkg) continue
      if (reqPkg.parent === tree && bd.indexOf(moduleName(reqPkg)) !== -1) {
        return true
      }
      requiredBy = union(requiredBy, reqPkg.package._requiredBy)
    }
    return false
  }

  new InstancePacker({ path: folder, type: 'Directory', isDirectory: true })
    .on('error', function (er) {
      if (er) log.error('tar pack', 'Error reading ' + folder)
      return cb(er)
    })

    // By default, npm includes some proprietary attributes in the
    // package tarball.  This is sane, and allowed by the spec.
    // However, npm *itself* excludes these from its own package,
    // so that it can be more easily bootstrapped using old and
    // non-compliant tar implementations.
    .pipe(tar.Pack({ noProprietary: !npm.config.get('proprietary-attribs') }))
    .on('error', function (er) {
      if (er) log.error('tar.pack', 'tar creation error', tarball)
      cb(er)
    })
    .pipe(zlib.Gzip())
    .on('error', function (er) {
      if (er) log.error('tar.pack', 'gzip error ' + tarball)
      cb(er)
    })
    .pipe(writeStreamAtomic(tarball))
    .on('error', function (er) {
      if (er) log.error('tar.pack', 'Could not write ' + tarball)
      cb(er)
    })
    .on('close', cb)
}

function unpack (tarball, unpackTarget, dMode, fMode, uid, gid, cb) {
  log.verbose('tar', 'unpack', tarball)
  log.verbose('tar', 'unpacking to', unpackTarget)
  if (typeof cb !== 'function') {
    cb = gid
    gid = null
  }
  if (typeof cb !== 'function') {
    cb = uid
    uid = null
  }
  if (typeof cb !== 'function') {
    cb = fMode
    fMode = npm.modes.file
  }
  if (typeof cb !== 'function') {
    cb = dMode
    dMode = npm.modes.exec
  }

  uidNumber(uid, gid, function (er, uid, gid) {
    if (er) return cb(er)
    unpack_(tarball, unpackTarget, dMode, fMode, uid, gid, cb)
  })
}

function unpack_ (tarball, unpackTarget, dMode, fMode, uid, gid, cb) {
  rm(unpackTarget, function (er) {
    if (er) return cb(er)
    // gzip {tarball} --decompress --stdout \
    //   | tar -mvxpf - --strip-components=1 -C {unpackTarget}
    gunzTarPerm(tarball, unpackTarget,
                dMode, fMode,
                uid, gid,
                function (er, folder) {
                  if (er) return cb(er)
                  readJson(path.resolve(folder, 'package.json'), cb)
                })
  })
}

function gunzTarPerm (tarball, target, dMode, fMode, uid, gid, cb_) {
  if (!dMode) dMode = npm.modes.exec
  if (!fMode) fMode = npm.modes.file
  log.silly('gunzTarPerm', 'modes', [dMode.toString(8), fMode.toString(8)])

  var cbCalled = false
  function cb (er) {
    if (cbCalled) return
    cbCalled = true
    cb_(er, target)
  }

  var fst = fs.createReadStream(tarball)

  fst.on('open', function (fd) {
    fs.fstat(fd, function (er, st) {
      if (er) return fst.emit('error', er)
      if (st.size === 0) {
        er = new Error('0-byte tarball\n' +
                       'Please run `npm cache clean`')
        fst.emit('error', er)
      }
    })
  })

  // figure out who we're supposed to be, if we're not pretending
  // to be a specific user.
  if (npm.config.get('unsafe-perm') && process.platform !== 'win32') {
    uid = myUid
    gid = myGid
  }

  function extractEntry (entry) {
    log.silly('gunzTarPerm', 'extractEntry', entry.path)
    // never create things that are user-unreadable,
    // or dirs that are user-un-listable. Only leads to headaches.
    var originalMode = entry.mode = entry.mode || entry.props.mode
    entry.mode = entry.mode | (entry.type === 'Directory' ? dMode : fMode)
    entry.mode = entry.mode & (~npm.modes.umask)
    entry.props.mode = entry.mode
    if (originalMode !== entry.mode) {
      log.silly('gunzTarPerm', 'modified mode',
                [entry.path, originalMode, entry.mode])
    }

    // if there's a specific owner uid/gid that we want, then set that
    if (process.platform !== 'win32' &&
        typeof uid === 'number' &&
        typeof gid === 'number') {
      entry.props.uid = entry.uid = uid
      entry.props.gid = entry.gid = gid
    }
  }

  var extractOpts = { type: 'Directory', path: target, strip: 1 }

  if (process.platform !== 'win32' &&
      typeof uid === 'number' &&
      typeof gid === 'number') {
    extractOpts.uid = uid
    extractOpts.gid = gid
  }

  var sawIgnores = {}
  extractOpts.filter = function () {
    // symbolic links are not allowed in packages.
    if (this.type.match(/^.*Link$/)) {
      log.warn('excluding symbolic link',
               this.path.substr(target.length + 1) +
               ' -> ' + this.linkpath)
      return false
    }

    // Note: This mirrors logic in the fs read operations that are
    // employed during tarball creation, in the fstream-npm module.
    // It is duplicated here to handle tarballs that are created
    // using other means, such as system tar or git archive.
    if (this.type === 'File') {
      var base = path.basename(this.path)
      if (base === '.npmignore') {
        sawIgnores[ this.path ] = true
      } else if (base === '.gitignore') {
        var npmignore = this.path.replace(/\.gitignore$/, '.npmignore')
        if (sawIgnores[npmignore]) {
          // Skip this one, already seen.
          return false
        } else {
          // Rename, may be clobbered later.
          this.path = npmignore
          this._path = npmignore
        }
      }
    }

    return true
  }

  fst
    .on('error', function (er) {
      if (er) log.error('tar.unpack', 'error reading ' + tarball)
      cb(er)
    })
    .on('data', function OD (c) {
      // detect what it is.
      // Then, depending on that, we'll figure out whether it's
      // a single-file module, gzipped tarball, or naked tarball.
      // gzipped files all start with 1f8b08
      if (c[0] === 0x1F &&
          c[1] === 0x8B &&
          c[2] === 0x08) {
        fst
          .pipe(zlib.Unzip())
          .on('error', function (er) {
            if (er) log.error('tar.unpack', 'unzip error ' + tarball)
            cb(er)
          })
          .pipe(tar.Extract(extractOpts))
          .on('entry', extractEntry)
          .on('error', function (er) {
            if (er) log.error('tar.unpack', 'untar error ' + tarball)
            cb(er)
          })
          .on('close', cb)
      } else if (hasTarHeader(c)) {
        // naked tar
        fst
          .pipe(tar.Extract(extractOpts))
          .on('entry', extractEntry)
          .on('error', function (er) {
            if (er) log.error('tar.unpack', 'untar error ' + tarball)
            cb(er)
          })
          .on('close', cb)
      } else {
        // naked js file
        var jsOpts = { path: path.resolve(target, 'index.js') }

        if (process.platform !== 'win32' &&
            typeof uid === 'number' &&
            typeof gid === 'number') {
          jsOpts.uid = uid
          jsOpts.gid = gid
        }

        fst
          .pipe(fstream.Writer(jsOpts))
          .on('error', function (er) {
            if (er) log.error('tar.unpack', 'copy error ' + tarball)
            cb(er)
          })
          .on('close', function () {
            var j = path.resolve(target, 'package.json')
            readJson(j, function (er, d) {
              if (er) {
                log.error('not a package', tarball)
                return cb(er)
              }
              writeFileAtomic(j, JSON.stringify(d) + '\n', cb)
            })
          })
      }

      // now un-hook, and re-emit the chunk
      fst.removeListener('data', OD)
      fst.emit('data', c)
    })
}

function hasTarHeader (c) {
  return c[257] === 0x75 && // tar archives have 7573746172 at position
         c[258] === 0x73 && // 257 and 003030 or 202000 at position 262
         c[259] === 0x74 &&
         c[260] === 0x61 &&
         c[261] === 0x72 &&

       ((c[262] === 0x00 &&
         c[263] === 0x30 &&
         c[264] === 0x30) ||

        (c[262] === 0x20 &&
         c[263] === 0x20 &&
         c[264] === 0x00))
}
