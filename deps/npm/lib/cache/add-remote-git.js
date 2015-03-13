var mkdir = require('mkdirp')
var assert = require('assert')
var git = require('../utils/git.js')
var fs = require('graceful-fs')
var log = require('npmlog')
var path = require('path')
var url = require('url')
var chownr = require('chownr')
var crypto = require('crypto')
var npm = require('../npm.js')
var rm = require('../utils/gently-rm.js')
var inflight = require('inflight')
var getCacheStat = require('./get-stat.js')
var addLocal = require('./add-local.js')
var realizePackageSpecifier = require('realize-package-specifier')
var normalizeGitUrl = require('normalize-git-url')
var randomBytes = require('crypto').pseudoRandomBytes // only need uniqueness

var remotes = path.resolve(npm.config.get('cache'), '_git-remotes')
var templates = path.join(remotes, '_templates')

var VALID_VARIABLES = [
  'GIT_SSH',
  'GIT_SSL_NO_VERIFY',
  'GIT_PROXY_COMMAND',
  'GIT_SSL_CAINFO'
]

module.exports = function addRemoteGit (uri, silent, cb) {
  assert(typeof uri === 'string', 'must have git URL')
  assert(typeof cb === 'function', 'must have callback')

  // reconstruct the URL as it was passed in – realizePackageSpecifier
  // strips off `git+` and `maybeGithub` doesn't include it.
  var originalURL
  if (!/^git[+:]/.test(uri)) {
    originalURL = 'git+' + uri
  } else {
    originalURL = uri
  }

  // break apart the origin URL and the branch / tag / commitish
  var normalized = normalizeGitUrl(uri)
  var gitURL = normalized.url
  var treeish = normalized.branch

  // ensure that similarly-named remotes don't collide
  var repoID = gitURL.replace(/[^a-zA-Z0-9]+/g, '-') + '-' +
    crypto.createHash('sha1').update(gitURL).digest('hex').slice(0, 8)
  var cachedRemote = path.join(remotes, repoID)

  // set later, as the callback flow proceeds
  var resolvedURL
  var resolvedTreeish
  var tmpdir

  cb = inflight(repoID, cb)
  if (!cb) {
    return log.verbose('addRemoteGit', repoID, 'already in flight; waiting')
  }
  log.verbose('addRemoteGit', repoID, 'not in flight; caching')

  // initialize the remotes cache with the correct perms
  getGitDir(function (er) {
    if (er) return cb(er)
    fs.stat(cachedRemote, function (er, s) {
      if (er) return mirrorRemote(finish)
      if (!s.isDirectory()) return resetRemote(finish)

      validateExistingRemote(finish)
    })

    // always set permissions on the cached remote
    function finish (er, data) {
      if (er) return cb(er, data)
      addModeRecursive(cachedRemote, npm.modes.file, function (er) {
        return cb(er, data)
      })
    }
  })

  // don't try too hard to hold on to a remote
  function resetRemote (cb) {
    log.info('addRemoteGit', 'resetting', cachedRemote)
    rm(cachedRemote, function (er) {
      if (er) return cb(er)
      mirrorRemote(cb)
    })
  }

  // reuse a cached remote when possible, but nuke it if it's in an
  // inconsistent state
  function validateExistingRemote (cb) {
    git.whichAndExec(
      ['config', '--get', 'remote.origin.url'],
      { cwd: cachedRemote, env: gitEnv() },
      function (er, stdout, stderr) {
        var originURL = stdout.trim()
        stderr = stderr.trim()
        log.verbose('addRemoteGit', 'remote.origin.url:', originURL)

        if (stderr || er) {
          log.warn('addRemoteGit', 'resetting remote', cachedRemote, 'because of error:', stderr || er)
          return resetRemote(cb)
        } else if (gitURL !== originURL) {
          log.warn(
            'addRemoteGit',
            'pre-existing cached repo', cachedRemote, 'points to', originURL, 'and not', gitURL
          )
          return resetRemote(cb)
        }

        log.verbose('addRemoteGit', 'updating existing cached remote', cachedRemote)
        updateRemote(cb)
      }
    )
  }

  // make a complete bare mirror of the remote repo
  // NOTE: npm uses a blank template directory to prevent weird inconsistencies
  // https://github.com/npm/npm/issues/5867
  function mirrorRemote (cb) {
    mkdir(cachedRemote, function (er) {
      if (er) return cb(er)

      var args = [
        'clone',
        '--template=' + templates,
        '--mirror',
        gitURL, cachedRemote
      ]
      git.whichAndExec(
        ['clone', '--template=' + templates, '--mirror', gitURL, cachedRemote],
        { cwd: cachedRemote, env: gitEnv() },
        function (er, stdout, stderr) {
          if (er) {
            var combined = (stdout + '\n' + stderr).trim()
            var command = 'git ' + args.join(' ') + ':'
            if (silent) {
              log.verbose(command, combined)
            } else {
              log.error(command, combined)
            }
            return cb(er)
          }
          log.verbose('addRemoteGit', 'git clone ' + gitURL, stdout.trim())
          setPermissions(cb)
        }
      )
    })
  }

  function setPermissions (cb) {
    if (process.platform === 'win32') {
      log.verbose('addRemoteGit', 'skipping chownr on Windows')
      resolveHead(cb)
    } else {
      getGitDir(function (er, cs) {
        if (er) {
          log.error('addRemoteGit', 'could not get cache stat')
          return cb(er)
        }

        chownr(cachedRemote, cs.uid, cs.gid, function (er) {
          if (er) {
            log.error(
              'addRemoteGit',
              'Failed to change folder ownership under npm cache for',
              cachedRemote
            )
            return cb(er)
          }

          log.verbose('addRemoteGit', 'set permissions on', cachedRemote)
          resolveHead(cb)
        })
      })
    }
  }

  // always fetch the origin, even right after mirroring, because this way
  // permissions will get set correctly
  function updateRemote (cb) {
    git.whichAndExec(
      ['fetch', '-a', 'origin'],
      { cwd: cachedRemote, env: gitEnv() },
      function (er, stdout, stderr) {
        if (er) {
          var combined = (stdout + '\n' + stderr).trim()
          log.error('git fetch -a origin (' + gitURL + ')', combined)
          return cb(er)
        }
        log.verbose('addRemoteGit', 'git fetch -a origin (' + gitURL + ')', stdout.trim())

        setPermissions(cb)
      }
    )
  }

  // branches and tags are both symbolic labels that can be attached to different
  // commits, so resolve the commitish to the current actual treeish the label
  // corresponds to
  //
  // important for shrinkwrap
  function resolveHead (cb) {
    log.verbose('addRemoteGit', 'original treeish:', treeish)
    var args = ['rev-list', '-n1', treeish]
    git.whichAndExec(
      args,
      { cwd: cachedRemote, env: gitEnv() },
      function (er, stdout, stderr) {
        if (er) {
          log.error('git ' + args.join(' ') + ':', stderr)
          return cb(er)
        }

        resolvedTreeish = stdout.trim()
        log.silly('addRemoteGit', 'resolved treeish:', resolvedTreeish)

        resolvedURL = getResolved(originalURL, resolvedTreeish)
        log.verbose('addRemoteGit', 'resolved Git URL:', resolvedURL)

        // generate a unique filename
        tmpdir = path.join(
          npm.tmp,
          'git-cache-' + randomBytes(6).toString('hex'),
          resolvedTreeish
        )
        log.silly('addRemoteGit', 'Git working directory:', tmpdir)

        mkdir(tmpdir, function (er) {
          if (er) return cb(er)

          cloneResolved(cb)
        })
      }
    )
  }

  // make a clone from the mirrored cache so we have a temporary directory in
  // which we can check out the resolved treeish
  function cloneResolved (cb) {
    var args = ['clone', cachedRemote, tmpdir]
    git.whichAndExec(
      args,
      { cwd: cachedRemote, env: gitEnv() },
      function (er, stdout, stderr) {
        stdout = (stdout + '\n' + stderr).trim()
        if (er) {
          log.error('git ' + args.join(' ') + ':', stderr)
          return cb(er)
        }
        log.verbose('addRemoteGit', 'clone', stdout)

        checkoutTreeish(cb)
      }
    )
  }

  // there is no safe way to do a one-step clone to a treeish that isn't
  // guaranteed to be a branch, so explicitly check out the treeish once it's
  // cloned
  function checkoutTreeish (cb) {
    var args = ['checkout', resolvedTreeish]
    git.whichAndExec(
      args,
      { cwd: tmpdir, env: gitEnv() },
      function (er, stdout, stderr) {
        stdout = (stdout + '\n' + stderr).trim()
        if (er) {
          log.error('git ' + args.join(' ') + ':', stderr)
          return cb(er)
        }
        log.verbose('addRemoteGit', 'checkout', stdout)

        // convince addLocal that the checkout is a local dependency
        realizePackageSpecifier(tmpdir, function (er, spec) {
          if (er) {
            log.error('addRemoteGit', 'Failed to map', tmpdir, 'to a package specifier')
            return cb(er)
          }

          // ensure pack logic is applied
          // https://github.com/npm/npm/issues/6400
          addLocal(spec, null, function (er, data) {
            if (data) {
              log.verbose('addRemoteGit', 'data._resolved:', resolvedURL)
              data._resolved = resolvedURL

              // the spec passed to addLocal is not what the user originally requested,
              // so remap
              // https://github.com/npm/npm/issues/7121
              if (!data._fromGitHub) {
                log.silly('addRemoteGit', 'data._from:', originalURL)
                data._from = originalURL
              } else {
                log.silly('addRemoteGit', 'data._from:', data._from, '(GitHub)')
              }
            }

            cb(er, data)
          })
        })
      }
    )
  }
}

function getGitDir (cb) {
  getCacheStat(function (er, stats) {
    if (er) return cb(er)

    // We don't need global templates when cloning. Use an empty directory for
    // the templates, creating it (and setting its permissions) if necessary.
    mkdir(templates, function (er) {
      if (er) return cb(er)

      // Ensure that both the template and remotes directories have the correct
      // permissions.
      fs.chown(templates, stats.uid, stats.gid, function (er) {
        if (er) return cb(er)

        fs.chown(remotes, stats.uid, stats.gid, function (er) {
          cb(er, stats)
        })
      })
    })
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

function getResolved (uri, treeish) {
  var parsed = url.parse(uri)
  parsed.hash = treeish
  if (!/^git[+:]/.test(parsed.protocol)) {
    parsed.protocol = 'git+' + parsed.protocol
  }
  var resolved = url.format(parsed)

  // node incorrectly sticks a / at the start of the path We know that the host
  // won't change, so split and detect this
  // https://github.com/npm/npm/issues/3224
  var spo = uri.split(parsed.host)
  var spr = resolved.split(parsed.host)
  if (spo[1].charAt(0) === ':' && spr[1].charAt(0) === '/') {
    spr[1] = spr[1].slice(1)
  }
  return spr.join(parsed.host)
}

// similar to chmodr except it add permissions rather than overwriting them
// adapted from https://github.com/isaacs/chmodr/blob/master/chmodr.js
function addModeRecursive (cachedRemote, mode, cb) {
  fs.readdir(cachedRemote, function (er, children) {
    // Any error other than ENOTDIR means it's not readable, or doesn't exist.
    // Give up.
    if (er && er.code !== 'ENOTDIR') return cb(er)
    if (er || !children.length) return addMode(cachedRemote, mode, cb)

    var len = children.length
    var errState = null
    children.forEach(function (child) {
      addModeRecursive(path.resolve(cachedRemote, child), mode, then)
    })

    function then (er) {
      if (errState) return undefined
      if (er) return cb(errState = er)
      if (--len === 0) return addMode(cachedRemote, dirMode(mode), cb)
    }
  })
}

function addMode (cachedRemote, mode, cb) {
  fs.stat(cachedRemote, function (er, stats) {
    if (er) return cb(er)
    mode = stats.mode | mode
    fs.chmod(cachedRemote, mode, cb)
  })
}

// taken from https://github.com/isaacs/chmodr/blob/master/chmodr.js
function dirMode (mode) {
  if (mode & parseInt('0400', 8)) mode |= parseInt('0100', 8)
  if (mode & parseInt('040', 8)) mode |= parseInt('010', 8)
  if (mode & parseInt('04', 8)) mode |= parseInt('01', 8)
  return mode
}
