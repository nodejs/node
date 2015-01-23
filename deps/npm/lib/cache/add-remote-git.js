var mkdir = require("mkdirp")
  , assert = require("assert")
  , git = require("../utils/git.js")
  , fs = require("graceful-fs")
  , log = require("npmlog")
  , path = require("path")
  , url = require("url")
  , chownr = require("chownr")
  , crypto = require("crypto")
  , npm = require("../npm.js")
  , rm = require("../utils/gently-rm.js")
  , inflight = require("inflight")
  , getCacheStat = require("./get-stat.js")
  , addLocal = require("./add-local.js")
  , realizePackageSpecifier = require("realize-package-specifier")
  , normalizeGitUrl = require("normalize-git-url")
  , randomBytes = require("crypto").pseudoRandomBytes // only need uniqueness

var remotes = path.resolve(npm.config.get("cache"), "_git-remotes")
var templates = path.join(remotes, "_templates")

var VALID_VARIABLES = [
  "GIT_SSH",
  "GIT_SSL_NO_VERIFY",
  "GIT_PROXY_COMMAND",
  "GIT_SSL_CAINFO"
]

// 1. cacheDir = path.join(cache,'_git-remotes',sha1(u))
// 2. checkGitDir(cacheDir) ? 4. : 3. (rm cacheDir if necessary)
// 3. git clone --mirror u cacheDir
// 4. cd cacheDir && git fetch -a origin
// 5. git archive /tmp/random.tgz
// 6. addLocalTarball(/tmp/random.tgz) <gitref> --format=tar --prefix=package/
// silent flag is used if this should error quietly
module.exports = function addRemoteGit (u, silent, cb) {
  assert(typeof u === "string", "must have git URL")
  assert(typeof cb === "function", "must have callback")

  log.verbose("addRemoteGit", "u=%j silent=%j", u, silent)
  var normalized = normalizeGitUrl(u)
  log.silly("addRemoteGit", "normalized", normalized)

  var v = crypto.createHash("sha1").update(normalized.url).digest("hex").slice(0, 8)
  v = normalized.url.replace(/[^a-zA-Z0-9]+/g, "-")+"-"+v
  log.silly("addRemoteGit", "v", v)

  var p = path.join(remotes, v)
  cb = inflight(p, cb)
  if (!cb) return log.verbose("addRemoteGit", p, "already in flight; waiting")
  log.verbose("addRemoteGit", p, "not in flight; cloning")

  getGitDir(function (er) {
    if (er) return cb(er)
    checkGitDir(p, normalized.url, normalized.branch, u, silent, function (er, data) {
      if (er) return cb(er, data)

      addModeRecursive(p, npm.modes.file, function (er) {
        return cb(er, data)
      })
    })
  })
}

function getGitDir (cb) {
  getCacheStat(function (er, st) {
    if (er) return cb(er)

    // We don't need global templates when cloning. Use an empty directory for
    // the templates, creating it (and setting its permissions) if necessary.
    mkdir(templates, function (er) {
      if (er) return cb(er)

      // Ensure that both the template and remotes directories have the correct
      // permissions.
      fs.chown(templates, st.uid, st.gid, function (er) {
        if (er) return cb(er)

        fs.chown(remotes, st.uid, st.gid, function (er) {
          cb(er, st)
        })
      })
    })
  })
}

function checkGitDir (p, u, co, origUrl, silent, cb) {
  fs.stat(p, function (er, s) {
    if (er) return cloneGitRemote(p, u, co, origUrl, silent, cb)
    if (!s.isDirectory()) return rm(p, function (er) {
      if (er) return cb(er)
      cloneGitRemote(p, u, co, origUrl, silent, cb)
    })

    git.whichAndExec(
      [ "config", "--get", "remote.origin.url" ],
      { cwd : p, env : gitEnv },
      function (er, stdout, stderr) {
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
        fetchRemote(p, u, co, origUrl, cb)
      }
    )
  })
}

function cloneGitRemote (p, u, co, origUrl, silent, cb) {
  mkdir(p, function (er) {
    if (er) return cb(er)

    git.whichAndExec(
      [ "clone", "--template=" + templates, "--mirror", u, p ],
      { cwd : p, env : gitEnv() },
      function (er, stdout, stderr) {
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
        fetchRemote(p, u, co, origUrl, cb)
      }
    )
  })
}

function fetchRemote (p, u, co, origUrl, cb) {
  git.whichAndExec(
    [ "fetch", "-a", "origin" ],
    { cwd : p, env : gitEnv() },
    function (er, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        log.error("git fetch -a origin ("+u+")", stdout)
        return cb(er)
      }
      log.verbose("git fetch -a origin ("+u+")", stdout)

      if (process.platform === "win32") {
        log.silly("verifyOwnership", "skipping for windows")
        resolveHead(p, u, co, origUrl, cb)
      }
      else {
        getGitDir(function (er, cs) {
          if (er) {
            log.error("Could not get cache stat")
            return cb(er)
          }

          chownr(p, cs.uid, cs.gid, function (er) {
            if (er) {
              log.error("Failed to change folder ownership under npm cache for %s", p)
              return cb(er)
            }

            resolveHead(p, u, co, origUrl, cb)
          })
        })
      }
    }
  )
}

function resolveHead (p, u, co, origUrl, cb) {
  git.whichAndExec(
    [ "rev-list", "-n1", co ],
    { cwd : p, env : gitEnv() },
    function (er, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        log.error("Failed resolving git HEAD (" + u + ")", stderr)
        return cb(er)
      }
      log.verbose("git rev-list -n1 " + co, stdout)
      var parsed = url.parse(origUrl)
      parsed.hash = stdout
      var resolved = url.format(parsed)

      if (parsed.protocol !== "git:") resolved = "git+" + resolved

      // https://github.com/npm/npm/issues/3224
      // node incorrectly sticks a / at the start of the path We know that the
      // host won't change, so split and detect this
      var spo = origUrl.split(parsed.host)
      var spr = resolved.split(parsed.host)
      if (spo[1].charAt(0) === ":" && spr[1].charAt(0) === "/") {
        spr[1] = spr[1].slice(1)
      }
      resolved = spr.join(parsed.host)

      log.verbose("resolved git url", resolved)
      cache(p, u, stdout, resolved, cb)
    }
  )
}

/**
 * Make an actual clone from the bare (mirrored) cache. There is no safe way to
 * do a one-step clone to a treeish that isn't guaranteed to be a branch, so
 * this has to be two steps.
 */
function cache (p, u, treeish, resolved, cb) {
  // generate a unique filename
  randomBytes(6, function (er, random) {
    if (er) return cb(er)

    var tmp = path.join(
      npm.tmp,
      "git-cache-"+random.toString("hex"),
      treeish
    )

    mkdir(tmp, function (er) {
      if (er) return cb(er)

      git.whichAndExec(["clone", p, tmp], { cwd : p, env : gitEnv() }, clone)
    })

    function clone (er, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        log.error("Failed to clone "+resolved+" from "+u, stderr)
        return cb(er)
      }
      log.verbose("git clone", "from", p)
      log.verbose("git clone", stdout)

      git.whichAndExec(["checkout", treeish], { cwd : tmp, env : gitEnv() }, checkout)
    }

    function checkout (er, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        log.error("Failed to check out "+treeish, stderr)
        return cb(er)
      }
      log.verbose("git checkout", stdout)

      realizePackageSpecifier(tmp, function (er, spec) {
        if (er) {
          log.error("Failed to map", tmp, "to a package specifier")
          return cb(er)
        }

        // https://github.com/npm/npm/issues/6400
        // ensure pack logic is applied
        addLocal(spec, null, function (er, data) {
          if (data) data._resolved = resolved
          cb(er, data)
        })
      })
    }
  })
}

var gitEnv_
function gitEnv () {
  // git responds to env vars in some weird ways in post-receive hooks
  // so don't carry those along.
  if (gitEnv_) return gitEnv_
  gitEnv_ = {}
  for (var k in process.env) {
    if (!~VALID_VARIABLES.indexOf(k) && k.match(/^GIT/)) continue
    gitEnv_[k] = process.env[k]
  }
  return gitEnv_
}

// similar to chmodr except it add permissions rather than overwriting them
// adapted from https://github.com/isaacs/chmodr/blob/master/chmodr.js
function addModeRecursive(p, mode, cb) {
  fs.readdir(p, function (er, children) {
    // Any error other than ENOTDIR means it's not readable, or doesn't exist.
    // Give up.
    if (er && er.code !== "ENOTDIR") return cb(er)
    if (er || !children.length) return addMode(p, mode, cb)

    var len = children.length
    var errState = null
    children.forEach(function (child) {
      addModeRecursive(path.resolve(p, child), mode, then)
    })

    function then (er) {
      if (errState) return undefined
      if (er) return cb(errState = er)
      if (--len === 0) return addMode(p, dirMode(mode), cb)
    }
  })
}

function addMode(p, mode, cb) {
  fs.stat(p, function (er, stats) {
    if (er) return cb(er)
    mode = stats.mode | mode
    fs.chmod(p, mode, cb)
  })
}

// taken from https://github.com/isaacs/chmodr/blob/master/chmodr.js
function dirMode(mode) {
  if (mode & parseInt("0400", 8)) mode |= parseInt("0100", 8)
  if (mode & parseInt( "040", 8)) mode |= parseInt( "010", 8)
  if (mode & parseInt(  "04", 8)) mode |= parseInt(  "01", 8)
  return mode
}
