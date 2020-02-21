'use strict'

const BB = require('bluebird')

const cp = require('child_process')
const execFileAsync = BB.promisify(cp.execFile, {
  multiArgs: true
})
const finished = require('./finished')
const LRU = require('lru-cache')
const optCheck = require('./opt-check')
const osenv = require('osenv')
const path = require('path')
const pinflight = require('promise-inflight')
const promiseRetry = require('promise-retry')
const uniqueFilename = require('unique-filename')
const which = BB.promisify(require('which'))
const semver = require('semver')
const inferOwner = require('infer-owner')

const GOOD_ENV_VARS = new Set([
  'GIT_ASKPASS',
  'GIT_EXEC_PATH',
  'GIT_PROXY_COMMAND',
  'GIT_SSH',
  'GIT_SSH_COMMAND',
  'GIT_SSL_CAINFO',
  'GIT_SSL_NO_VERIFY'
])

const GIT_TRANSIENT_ERRORS = [
  'remote error: Internal Server Error',
  'The remote end hung up unexpectedly',
  'Connection timed out',
  'Operation timed out',
  'Failed to connect to .* Timed out',
  'Connection reset by peer',
  'SSL_ERROR_SYSCALL',
  'The requested URL returned error: 503'
].join('|')

const GIT_TRANSIENT_ERROR_RE = new RegExp(GIT_TRANSIENT_ERRORS)

const GIT_TRANSIENT_ERROR_MAX_RETRY_NUMBER = 3

function shouldRetry (error, number) {
  return GIT_TRANSIENT_ERROR_RE.test(error) && (number < GIT_TRANSIENT_ERROR_MAX_RETRY_NUMBER)
}

const GIT_ = 'GIT_'
let GITENV
function gitEnv () {
  if (GITENV) { return GITENV }
  const tmpDir = path.join(osenv.tmpdir(), 'pacote-git-template-tmp')
  const tmpName = uniqueFilename(tmpDir, 'git-clone')
  GITENV = {
    GIT_ASKPASS: 'echo',
    GIT_TEMPLATE_DIR: tmpName
  }
  Object.keys(process.env).forEach(k => {
    if (GOOD_ENV_VARS.has(k) || !k.startsWith(GIT_)) {
      GITENV[k] = process.env[k]
    }
  })
  return GITENV
}

let GITPATH
try {
  GITPATH = which.sync('git')
} catch (e) {}

module.exports.clone = fullClone
function fullClone (repo, committish, target, opts) {
  opts = optCheck(opts)
  const gitArgs = ['clone', '--mirror', '-q', repo, path.join(target, '.git')]
  if (process.platform === 'win32') {
    gitArgs.push('--config', 'core.longpaths=true')
  }
  return execGit(gitArgs, { cwd: target }, opts).then(() => {
    return execGit(['init'], { cwd: target }, opts)
  }).then(() => {
    return execGit(['checkout', committish || 'HEAD'], { cwd: target }, opts)
  }).then(() => {
    return updateSubmodules(target, opts)
  }).then(() => headSha(target, opts))
}

module.exports.shallow = shallowClone
function shallowClone (repo, branch, target, opts) {
  opts = optCheck(opts)
  const gitArgs = ['clone', '--depth=1', '-q']
  if (branch) {
    gitArgs.push('-b', branch)
  }
  gitArgs.push(repo, target)
  if (process.platform === 'win32') {
    gitArgs.push('--config', 'core.longpaths=true')
  }
  return execGit(gitArgs, {
    cwd: target
  }, opts).then(() => {
    return updateSubmodules(target, opts)
  }).then(() => headSha(target, opts))
}

function updateSubmodules (localRepo, opts) {
  const gitArgs = ['submodule', 'update', '-q', '--init', '--recursive']
  return execGit(gitArgs, {
    cwd: localRepo
  }, opts)
}

function headSha (repo, opts) {
  opts = optCheck(opts)
  return execGit(['rev-parse', '--revs-only', 'HEAD'], { cwd: repo }, opts).spread(stdout => {
    return stdout.trim()
  })
}

const CARET_BRACES = '^{}'
const REVS = new LRU({
  max: 100,
  maxAge: 5 * 60 * 1000
})
module.exports.revs = revs
function revs (repo, opts) {
  opts = optCheck(opts)
  const cached = REVS.get(repo)
  if (cached) {
    return BB.resolve(cached)
  }
  return pinflight(`ls-remote:${repo}`, () => {
    return spawnGit(['ls-remote', '-h', '-t', repo], {
      env: gitEnv()
    }, opts).then((stdout) => {
      return stdout.split('\n').reduce((revs, line) => {
        const split = line.split(/\s+/, 2)
        if (split.length < 2) { return revs }
        const sha = split[0].trim()
        const ref = split[1].trim().match(/(?:refs\/[^/]+\/)?(.*)/)[1]
        if (!ref) { return revs } // ???
        if (ref.endsWith(CARET_BRACES)) { return revs } // refs/tags/x^{} crap
        const type = refType(line)
        const doc = { sha, ref, type }

        revs.refs[ref] = doc
        // We can check out shallow clones on specific SHAs if we have a ref
        if (revs.shas[sha]) {
          revs.shas[sha].push(ref)
        } else {
          revs.shas[sha] = [ref]
        }

        if (type === 'tag') {
          const match = ref.match(/v?(\d+\.\d+\.\d+(?:[-+].+)?)$/)
          if (match && semver.valid(match[1], true)) {
            revs.versions[semver.clean(match[1], true)] = doc
          }
        }

        return revs
      }, { versions: {}, 'dist-tags': {}, refs: {}, shas: {} })
    }, err => {
      err.message = `Error while executing:\n${GITPATH} ls-remote -h -t ${repo}\n\n${err.stderr}\n${err.message}`
      throw err
    }).then(revs => {
      if (revs.refs.HEAD) {
        const HEAD = revs.refs.HEAD
        Object.keys(revs.versions).forEach(v => {
          if (v.sha === HEAD.sha) {
            revs['dist-tags'].HEAD = v
            if (!revs.refs.latest) {
              revs['dist-tags'].latest = revs.refs.HEAD
            }
          }
        })
      }
      REVS.set(repo, revs)
      return revs
    })
  })
}

// infer the owner from the cwd git is operating in, if not the
// process cwd, but only if we're root.
// See: https://github.com/npm/cli/issues/624
module.exports._cwdOwner = cwdOwner
function cwdOwner (gitOpts, opts) {
  const isRoot = process.getuid && process.getuid() === 0
  if (!isRoot || !gitOpts.cwd) { return Promise.resolve() }

  return BB.resolve(inferOwner(gitOpts.cwd).then(owner => {
    gitOpts.uid = owner.uid
    gitOpts.gid = owner.gid
  }))
}

module.exports._exec = execGit
function execGit (gitArgs, gitOpts, opts) {
  opts = optCheck(opts)
  return BB.resolve(cwdOwner(gitOpts, opts).then(() => checkGit(opts).then(gitPath => {
    return promiseRetry((retry, number) => {
      if (number !== 1) {
        opts.log.silly('pacote', 'Retrying git command: ' + gitArgs.join(' ') + ' attempt # ' + number)
      }
      return execFileAsync(gitPath, gitArgs, mkOpts(gitOpts, opts)).catch((err) => {
        if (shouldRetry(err, number)) {
          retry(err)
        } else {
          throw err
        }
      })
    }, opts.retry != null ? opts.retry : {
      retries: opts['fetch-retries'],
      factor: opts['fetch-retry-factor'],
      maxTimeout: opts['fetch-retry-maxtimeout'],
      minTimeout: opts['fetch-retry-mintimeout']
    })
  })))
}

module.exports._spawn = spawnGit
function spawnGit (gitArgs, gitOpts, opts) {
  opts = optCheck(opts)
  return BB.resolve(cwdOwner(gitOpts, opts).then(() => checkGit(opts).then(gitPath => {
    return promiseRetry((retry, number) => {
      if (number !== 1) {
        opts.log.silly('pacote', 'Retrying git command: ' + gitArgs.join(' ') + ' attempt # ' + number)
      }
      const child = cp.spawn(gitPath, gitArgs, mkOpts(gitOpts, opts))

      let stdout = ''
      let stderr = ''
      child.stdout.on('data', d => { stdout += d })
      child.stderr.on('data', d => { stderr += d })

      return finished(child, true).catch(err => {
        if (shouldRetry(stderr, number)) {
          retry(err)
        } else {
          err.stderr = stderr
          throw err
        }
      }).then(() => {
        return stdout
      })
    }, opts.retry)
  })))
}

module.exports._mkOpts = mkOpts
function mkOpts (_gitOpts, opts) {
  const gitOpts = {
    env: gitEnv()
  }
  const isRoot = process.getuid && process.getuid() === 0
  // don't change child process uid/gid if not root
  if (+opts.uid && !isNaN(opts.uid) && isRoot) {
    gitOpts.uid = +opts.uid
  }
  if (+opts.gid && !isNaN(opts.gid) && isRoot) {
    gitOpts.gid = +opts.gid
  }
  Object.assign(gitOpts, _gitOpts)
  return gitOpts
}

function checkGit (opts) {
  if (opts.git) {
    return BB.resolve(opts.git)
  } else if (!GITPATH) {
    const err = new Error('No git binary found in $PATH')
    err.code = 'ENOGIT'
    return BB.reject(err)
  } else {
    return BB.resolve(GITPATH)
  }
}

const REFS_TAGS = 'refs/tags/'
const REFS_HEADS = 'refs/heads/'
const HEAD = 'HEAD'
function refType (ref) {
  return ref.indexOf(REFS_TAGS) !== -1
    ? 'tag'
    : ref.indexOf(REFS_HEADS) !== -1
      ? 'branch'
      : ref.endsWith(HEAD)
        ? 'head'
        : 'other'
}
