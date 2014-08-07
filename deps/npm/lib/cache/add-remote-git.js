var mkdir = require("mkdirp")
  , assert = require("assert")
  , git = require("../utils/git.js")
  , once = require("once")
  , fs = require("graceful-fs")
  , log = require("npmlog")
  , path = require("path")
  , url = require("url")
  , chownr = require("chownr")
  , zlib = require("zlib")
  , which = require("which")
  , crypto = require("crypto")
  , chmodr = require("chmodr")
  , npm = require("../npm.js")
  , rm = require("../utils/gently-rm.js")
  , inflight = require("inflight")
  , locker = require("../utils/locker.js")
  , lock = locker.lock
  , unlock = locker.unlock
  , getCacheStat = require("./get-stat.js")
  , addLocalTarball = require("./add-local-tarball.js")


// 1. cacheDir = path.join(cache,'_git-remotes',sha1(u))
// 2. checkGitDir(cacheDir) ? 4. : 3. (rm cacheDir if necessary)
// 3. git clone --mirror u cacheDir
// 4. cd cacheDir && git fetch -a origin
// 5. git archive /tmp/random.tgz
// 6. addLocalTarball(/tmp/random.tgz) <gitref> --format=tar --prefix=package/
// silent flag is used if this should error quietly
module.exports = function addRemoteGit (u, parsed, silent, cb_) {
  assert(typeof u === "string", "must have git URL")
  assert(typeof parsed === "object", "must have parsed query")
  assert(typeof cb_ === "function", "must have callback")

  function cb (er, data) {
    unlock(u, function () { cb_(er, data) })
  }

  cb_ = inflight(u, cb_)

  if (!cb_) return

  // git is so tricky!
  // if the path is like ssh://foo:22/some/path then it works, but
  // it needs the ssh://
  // If the path is like ssh://foo:some/path then it works, but
  // only if you remove the ssh://
  var origUrl = u
  u = u.replace(/^git\+/, "")
       .replace(/#.*$/, "")

  // ssh paths that are scp-style urls don't need the ssh://
  if (parsed.pathname.match(/^\/?:/)) {
    u = u.replace(/^ssh:\/\//, "")
  }

  lock(u, function (er) {
    if (er) return cb(er)

    // figure out what we should check out.
    var co = parsed.hash && parsed.hash.substr(1) || "master"

    var v = crypto.createHash("sha1").update(u).digest("hex").slice(0, 8)
    v = u.replace(/[^a-zA-Z0-9]+/g, '-') + '-' + v

    log.verbose("addRemoteGit", [u, co])

    var p = path.join(npm.config.get("cache"), "_git-remotes", v)

    checkGitDir(p, u, co, origUrl, silent, function(er, data) {
      chmodr(p, npm.modes.file, function(erChmod) {
        if (er) return cb(er, data)
        return cb(erChmod, data)
      })
    })
  })
}

function checkGitDir (p, u, co, origUrl, silent, cb) {
  fs.stat(p, function (er, s) {
    if (er) return cloneGitRemote(p, u, co, origUrl, silent, cb)
    if (!s.isDirectory()) return rm(p, function (er){
      if (er) return cb(er)
      cloneGitRemote(p, u, co, origUrl, silent, cb)
    })

    var args = [ "config", "--get", "remote.origin.url" ]
    var env = gitEnv()

    // check for git
    git.whichAndExec(args, {cwd: p, env: env}, function (er, stdout, stderr) {
      var stdoutTrimmed = (stdout + "\n" + stderr).trim()
      if (er || u !== stdout.trim()) {
        log.warn( "`git config --get remote.origin.url` returned "
                + "wrong result ("+u+")", stdoutTrimmed )
        return rm(p, function (er){
          if (er) return cb(er)
          cloneGitRemote(p, u, co, origUrl, silent, cb)
        })
      }
      log.verbose("git remote.origin.url", stdoutTrimmed)
      archiveGitRemote(p, u, co, origUrl, cb)
    })
  })
}

function cloneGitRemote (p, u, co, origUrl, silent, cb) {
  mkdir(p, function (er) {
    if (er) return cb(er)

    var args = [ "clone", "--mirror", u, p ]
    var env = gitEnv()

    // check for git
    git.whichAndExec(args, {cwd: p, env: env}, function (er, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        if (silent) {
          log.verbose("git clone " + u, stdout)
        } else {
          log.error("git clone " + u, stdout)
        }
        return cb(er)
      }
      log.verbose("git clone " + u, stdout)
      archiveGitRemote(p, u, co, origUrl, cb)
    })
  })
}

function archiveGitRemote (p, u, co, origUrl, cb) {
  var archive = [ "fetch", "-a", "origin" ]
  var resolve = [ "rev-list", "-n1", co ]
  var env = gitEnv()

  var resolved = null
  var tmp

  git.whichAndExec(archive, {cwd: p, env: env}, function (er, stdout, stderr) {
    stdout = (stdout + "\n" + stderr).trim()
    if (er) {
      log.error("git fetch -a origin ("+u+")", stdout)
      return cb(er)
    }
    log.verbose("git fetch -a origin ("+u+")", stdout)
    tmp = path.join(npm.tmp, Date.now()+"-"+Math.random(), "tmp.tgz")
    verifyOwnership()
  })

  function verifyOwnership() {
    if (process.platform === "win32") {
      log.silly("verifyOwnership", "skipping for windows")
      resolveHead()
    } else {
      getCacheStat(function(er, cs) {
        if (er) {
          log.error("Could not get cache stat")
          return cb(er)
        }
        chownr(p, cs.uid, cs.gid, function(er) {
          if (er) {
            log.error("Failed to change folder ownership under npm cache for %s", p)
            return cb(er)
          }
          resolveHead()
        })
      })
    }
  }

  function resolveHead () {
    git.whichAndExec(resolve, {cwd: p, env: env}, function (er, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        log.error("Failed resolving git HEAD (" + u + ")", stderr)
        return cb(er)
      }
      log.verbose("git rev-list -n1 " + co, stdout)
      var parsed = url.parse(origUrl)
      parsed.hash = stdout
      resolved = url.format(parsed)

      // https://github.com/npm/npm/issues/3224
      // node incorrectly sticks a / at the start of the path
      // We know that the host won't change, so split and detect this
      var spo = origUrl.split(parsed.host)
      var spr = resolved.split(parsed.host)
      if (spo[1].charAt(0) === ':' && spr[1].charAt(0) === '/')
        spr[1] = spr[1].slice(1)
      resolved = spr.join(parsed.host)

      log.verbose('resolved git url', resolved)
      next()
    })
  }

  function next () {
    mkdir(path.dirname(tmp), function (er) {
      if (er) return cb(er)
      var gzip = zlib.createGzip({ level: 9 })
      var args = ["archive", co, "--format=tar", "--prefix=package/"]
      var out = fs.createWriteStream(tmp)
      var env = gitEnv()
      cb = once(cb)
      var cp = git.spawn(args, { env: env, cwd: p })
      cp.on("error", cb)
      cp.stderr.on("data", function(chunk) {
        log.silly(chunk.toString(), "git archive")
      })

      cp.stdout.pipe(gzip).pipe(out).on("close", function() {
        addLocalTarball(tmp, null, null, function(er, data) {
          if (data) data._resolved = resolved
          cb(er, data)
        })
      })
    })
  }
}

var gitEnv_
function gitEnv () {
  // git responds to env vars in some weird ways in post-receive hooks
  // so don't carry those along.
  if (gitEnv_) return gitEnv_
  gitEnv_ = {}
  for (var k in process.env) {
    if (!~['GIT_PROXY_COMMAND','GIT_SSH','GIT_SSL_NO_VERIFY'].indexOf(k) && k.match(/^GIT/)) continue
    gitEnv_[k] = process.env[k]
  }
  return gitEnv_
}
