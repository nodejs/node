var fs = require("fs")
var path = require("path")
var log = require("npmlog")
var semver = require("semver")

exports.checkEngine = checkEngine
function checkEngine (target, npmVer, nodeVer, force, strict, cb) {
  var nodev = force ? null : nodeVer
    , eng = target.engines
  if (!eng) return cb()
  if (nodev && eng.node && !semver.satisfies(nodev, eng.node)
      || eng.npm && !semver.satisfies(npmVer, eng.npm)) {

    if (strict) {
      var er = new Error("Unsupported")
      er.code = "ENOTSUP"
      er.required = eng
      er.pkgid = target._id
      return cb(er)
    } else {
      log.warn( "engine", "%s: wanted: %j (current: %j)"
              , target._id, eng, {node: nodev, npm: npmVer} )
    }
  }
  return cb()
}

exports.checkPlatform = checkPlatform
function checkPlatform (target, force, cb) {
  var platform = process.platform
    , arch = process.arch
    , osOk = true
    , cpuOk = true

  if (force) {
    return cb()
  }

  if (target.os) {
    osOk = checkList(platform, target.os)
  }
  if (target.cpu) {
    cpuOk = checkList(arch, target.cpu)
  }
  if (!osOk || !cpuOk) {
    var er = new Error("Unsupported")
    er.code = "EBADPLATFORM"
    er.os = target.os || ['any']
    er.cpu = target.cpu || ['any']
    er.pkgid = target._id
    return cb(er)
  }
  return cb()
}

function checkList (value, list) {
  var tmp
    , match = false
    , blc = 0
  if (typeof list === "string") {
    list = [list]
  }
  if (list.length === 1 && list[0] === "any") {
    return true
  }
  for (var i = 0; i < list.length; ++i) {
    tmp = list[i]
    if (tmp[0] === '!') {
      tmp = tmp.slice(1)
      if (tmp === value) {
        return false
      }
      ++blc
    } else {
      match = match || tmp === value
    }
  }
  return match || blc === list.length
}

exports.checkCycle = checkCycle
function checkCycle (target, ancestors, cb) {
  // there are some very rare and pathological edge-cases where
  // a cycle can cause npm to try to install a never-ending tree
  // of stuff.
  // Simplest:
  //
  // A -> B -> A' -> B' -> A -> B -> A' -> B' -> A -> ...
  //
  // Solution: Simply flat-out refuse to install any name@version
  // that is already in the prototype tree of the ancestors object.
  // A more correct, but more complex, solution would be to symlink
  // the deeper thing into the new location.
  // Will do that if anyone whines about this irl.
  //
  // Note: `npm install foo` inside of the `foo` package will abort
  // earlier if `--force` is not set.  However, if it IS set, then
  // we need to still fail here, but just skip the first level. Of
  // course, it'll still fail eventually if it's a true cycle, and
  // leave things in an undefined state, but that's what is to be
  // expected when `--force` is used.  That is why getPrototypeOf
  // is used *twice* here: to skip the first level of repetition.

  var p = Object.getPrototypeOf(Object.getPrototypeOf(ancestors))
    , name = target.name
    , version = target.version
  while (p && p !== Object.prototype && p[name] !== version) {
    p = Object.getPrototypeOf(p)
  }
  if (p[name] !== version) return cb()

  var er = new Error("Unresolvable cycle detected")
  var tree = [target._id, JSON.parse(JSON.stringify(ancestors))]
    , t = Object.getPrototypeOf(ancestors)
  while (t && t !== Object.prototype) {
    if (t === p) t.THIS_IS_P = true
    tree.push(JSON.parse(JSON.stringify(t)))
    t = Object.getPrototypeOf(t)
  }
  log.verbose("unresolvable dependency tree", tree)
  er.pkgid = target._id
  er.code = "ECYCLE"
  return cb(er)
}

exports.checkGit = checkGit
function checkGit (folder, cb) {
  // if it's a git repo then don't touch it!
  fs.lstat(folder, function (er, s) {
    if (er || !s.isDirectory()) return cb()
    else checkGit_(folder, cb)
  })
}

function checkGit_ (folder, cb) {
  fs.stat(path.resolve(folder, ".git"), function (er, s) {
    if (!er && s.isDirectory()) {
      var e = new Error("Appears to be a git repo or submodule.")
      e.path = folder
      e.code = "EISGIT"
      return cb(e)
    }
    cb()
  })
}
