// link with no args: symlink the folder to the global location
// link with package arg: symlink the global to the local

var npm = require('./npm.js')
var symlink = require('./utils/link.js')
var fs = require('graceful-fs')
var log = require('npmlog')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var path = require('path')
var build = require('./build.js')
var npa = require('npm-package-arg')
var usage = require('./utils/usage')
var output = require('./utils/output.js')

module.exports = link

link.usage = usage(
  'link',
  'npm link (in package dir)' +
  '\nnpm link [<@scope>/]<pkg>[@<version>]'
)

link.completion = function (opts, cb) {
  var dir = npm.globalDir
  fs.readdir(dir, function (er, files) {
    cb(er, files.filter(function (f) {
      return !f.match(/^[\._-]/)
    }))
  })
}

function link (args, cb) {
  if (process.platform === 'win32') {
    var semver = require('semver')
    if (!semver.gte(process.version, '0.7.9')) {
      var msg = 'npm link not supported on windows prior to node 0.7.9'
      var e = new Error(msg)
      e.code = 'ENOTSUP'
      e.errno = require('constants').ENOTSUP
      return cb(e)
    }
  }

  if (npm.config.get('global')) {
    return cb(new Error(
      'link should never be --global.\n' +
      'Please re-run this command with --local'
    ))
  }

  if (args.length === 1 && args[0] === '.') args = []
  if (args.length) return linkInstall(args, cb)
  linkPkg(npm.prefix, cb)
}

function parentFolder (id, folder) {
  if (id[0] === '@') {
    return path.resolve(folder, '..', '..')
  } else {
    return path.resolve(folder, '..')
  }
}

function linkInstall (pkgs, cb) {
  asyncMap(pkgs, function (pkg, cb) {
    var t = path.resolve(npm.globalDir, '..')
    var pp = path.resolve(npm.globalDir, pkg)
    var rp = null
    var target = path.resolve(npm.dir, pkg)

    function n (er, data) {
      if (er) return cb(er, data)
      // we want the ONE thing that was installed into the global dir
      var installed = data.filter(function (info) {
        var id = info[0]
        var folder = info[1]
        return parentFolder(id, folder) === npm.globalDir
      })
      var id = installed[0][0]
      pp = installed[0][1]
      var what = npa(id)
      pkg = what.name
      target = path.resolve(npm.dir, pkg)
      next()
    }

    // if it's a folder, a random not-installed thing, or not a scoped package,
    // then link or install it first
    if (pkg[0] !== '@' && (pkg.indexOf('/') !== -1 || pkg.indexOf('\\') !== -1)) {
      return fs.lstat(path.resolve(pkg), function (er, st) {
        if (er || !st.isDirectory()) {
          npm.commands.install(t, pkg, n)
        } else {
          rp = path.resolve(pkg)
          linkPkg(rp, n)
        }
      })
    }

    fs.lstat(pp, function (er, st) {
      if (er) {
        rp = pp
        return npm.commands.install(t, [pkg], n)
      } else if (!st.isSymbolicLink()) {
        rp = pp
        next()
      } else {
        return fs.realpath(pp, function (er, real) {
          if (er) log.warn('invalid symbolic link', pkg)
          else rp = real
          next()
        })
      }
    })

    function next () {
      if (npm.config.get('dry-run')) return resultPrinter(pkg, pp, target, rp, cb)
      chain(
        [
          [ function (cb) {
            log.verbose('link', 'symlinking %s to %s', pp, target)
            cb()
          } ],
          [symlink, pp, target, false, false],
          // do not run any scripts
          rp && [build, [target], npm.config.get('global'), build._noLC, true],
          [resultPrinter, pkg, pp, target, rp]
        ],
        cb
      )
    }
  }, cb)
}

function linkPkg (folder, cb_) {
  var me = folder || npm.prefix
  var readJson = require('read-package-json')

  log.verbose('linkPkg', folder)

  readJson(path.resolve(me, 'package.json'), function (er, d) {
    function cb (er) {
      return cb_(er, [[d && d._id, target, null, null]])
    }
    if (er) return cb(er)
    if (!d.name) {
      er = new Error('Package must have a name field to be linked')
      return cb(er)
    }
    if (npm.config.get('dry-run')) return resultPrinter(path.basename(me), me, target, cb)
    var target = path.resolve(npm.globalDir, d.name)
    symlink(me, target, false, true, function (er) {
      if (er) return cb(er)
      log.verbose('link', 'build target', target)
      // also install missing dependencies.
      npm.commands.install(me, [], function (er) {
        if (er) return cb(er)
        // build the global stuff.  Don't run *any* scripts, because
        // install command already will have done that.
        build([target], true, build._noLC, true, function (er) {
          if (er) return cb(er)
          resultPrinter(path.basename(me), me, target, cb)
        })
      })
    })
  })
}

function resultPrinter (pkg, src, dest, rp, cb) {
  if (typeof cb !== 'function') {
    cb = rp
    rp = null
  }
  var where = dest
  rp = (rp || '').trim()
  src = (src || '').trim()
  // XXX If --json is set, then look up the data from the package.json
  if (npm.config.get('parseable')) {
    return parseableOutput(dest, rp || src, cb)
  }
  if (rp === src) rp = null
  output(where + ' -> ' + src + (rp ? ' -> ' + rp : ''))
  cb()
}

function parseableOutput (dest, rp, cb) {
  // XXX this should match ls --parseable and install --parseable
  // look up the data from package.json, format it the same way.
  //
  // link is always effectively 'long', since it doesn't help much to
  // *just* print the target folder.
  // However, we don't actually ever read the version number, so
  // the second field is always blank.
  output(dest + '::' + rp)
  cb()
}
