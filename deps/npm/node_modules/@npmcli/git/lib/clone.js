// The goal here is to minimize both git workload and
// the number of refs we download over the network.
//
// Every method ends up with the checked out working dir
// at the specified ref, and resolves with the git sha.

// Only certain whitelisted hosts get shallow cloning.
// Many hosts (including GHE) don't always support it.
// A failed shallow fetch takes a LOT longer than a full
// fetch in most cases, so we skip it entirely.
// Set opts.gitShallow = true/false to force this behavior
// one way or the other.
const shallowHosts = new Set([
  'github.com',
  'gist.github.com',
  'gitlab.com',
  'bitbucket.com',
  'bitbucket.org',
])
const { parse } = require('url')
const { basename, resolve } = require('path')

const revs = require('./revs.js')
const spawn = require('./spawn.js')

const pickManifest = require('npm-pick-manifest')
const { promisify } = require('util')
const fs = require('fs')
const mkdirp = require('mkdirp')

module.exports = (repo, ref = 'HEAD', target = null, /* istanbul ignore next */ opts = {}) =>
  revs(repo, opts).then(revs => clone(
    repo,
    revs,
    ref,
    resolveRef(revs, ref, opts),
    target || defaultTarget(repo, opts.cwd),
    opts
  ))

const maybeShallow = (repo, opts) =>
  opts.gitShallow === false || opts.gitShallow ? opts.gitShallow
    : shallowHosts.has(parse(repo).host)

const isWindows = opts => (opts.fakePlatform || process.platform) === 'win32'

const defaultTarget = (repo, /* istanbul ignore next */ cwd = process.cwd()) =>
  resolve(cwd, basename(repo.replace(/[\/\\]?\.git$/, '')))

const clone = (repo, revs, ref, revDoc, target, opts) =>
  !revDoc ? unresolved(repo, ref, target, opts)
    : revDoc.sha === revs.refs.HEAD.sha ? plain(repo, revDoc, target, opts)
    : revDoc.type === 'tag' || revDoc.type === 'branch'
      ? branch(repo, revDoc, target, opts)
    : other(repo, revDoc, target, opts)

const resolveRef = (revs, ref, opts) => {
  const { spec = {} } = opts
  ref = spec.gitCommittish || ref
  return !revs ? /* istanbul ignore next - will fail anyway, can't pull */ null
    : spec.gitRange ? pickManifest(revs, spec.gitRange, opts)
    : !ref ? revs.refs.HEAD
    : revs.refs[ref] ? revs.refs[ref]
    : revs.shas[ref] ? revs.refs[revs.shas[ref][0]]
    : null
}

// pull request or some other kind of advertised ref
const other = (repo, revDoc, target, opts) => {
  const shallow = maybeShallow(repo, opts)

  const fetchOrigin = [ 'fetch', 'origin', revDoc.rawRef ]
    .concat(shallow ? ['--depth=1'] : [])

  const git = (args) => spawn(args, { ...opts, cwd: target })
  return mkdirp(target)
    .then(() => git(['init']))
    .then(() => isWindows(opts)
      ? git(['config', '--local', '--add', 'core.longpaths', 'true'])
      : null)
    .then(() => git(['remote', 'add', 'origin', repo]))
    .then(() => git(fetchOrigin))
    .then(() => git(['checkout', revDoc.sha]))
    .then(() => updateSubmodules(target, opts))
    .then(() => revDoc.sha)
}

// tag or branches.  use -b
const branch = (repo, revDoc, target, opts) => {
  const args = [
    'clone',
    '-b',
    revDoc.ref,
    repo,
    target,
    '--recurse-submodules',
  ]
  if (maybeShallow(repo, opts))
    args.push('--depth=1')
  if (isWindows(opts))
    args.push('--config', 'core.longpaths=true')
  return spawn(args, opts).then(() => revDoc.sha)
}

// just the head.  clone it
const plain = (repo, revDoc, target, opts) => {
  const args = [
    'clone',
    repo,
    target,
    '--recurse-submodules'
  ]
  if (maybeShallow(repo, opts))
    args.push('--depth=1')
  if (isWindows(opts))
    args.push('--config', 'core.longpaths=true')
  return spawn(args, opts).then(() => revDoc.sha)
}

const updateSubmodules = (target, opts) => new Promise(res =>
  fs.stat(target + '/.gitmodules', er => res(er ? null
    : spawn([
      'submodule',
      'update',
      '-q',
      '--init',
      '--recursive'
    ], { ...opts, cwd: target }))))

const unresolved = (repo, ref, target, opts) => {
  // can't do this one shallowly, because the ref isn't advertised
  // but we can avoid checking out the working dir twice, at least
  const lp = isWindows(opts) ? ['--config', 'core.longpaths=true'] : []
  const cloneArgs = ['clone', '--mirror', '-q', repo, target + '/.git']
  const git = (args) => spawn(args, { ...opts, cwd: target })
  return mkdirp(target)
    .then(() => git(cloneArgs.concat(lp)))
    .then(() => git(['init']))
    .then(() => git(['checkout', ref]))
    .then(() => updateSubmodules(target, opts))
    .then(() => git(['rev-parse', '--revs-only', 'HEAD']))
    .then(({stdout}) => stdout.trim())
}
