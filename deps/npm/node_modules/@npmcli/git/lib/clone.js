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
// we have to use url.parse until we add the same shim that hosted-git-info has
// to handle scp:// urls
const { parse } = require('url') // eslint-disable-line node/no-deprecated-api
const { basename, resolve } = require('path')

const revs = require('./revs.js')
const spawn = require('./spawn.js')
const { isWindows } = require('./utils.js')

const pickManifest = require('npm-pick-manifest')
const fs = require('fs')
const mkdirp = require('mkdirp')

module.exports = (repo, ref = 'HEAD', target = null, opts = {}) =>
  revs(repo, opts).then(revs => clone(
    repo,
    revs,
    ref,
    resolveRef(revs, ref, opts),
    target || defaultTarget(repo, opts.cwd),
    opts
  ))

const maybeShallow = (repo, opts) => {
  if (opts.gitShallow === false || opts.gitShallow) {
    return opts.gitShallow
  }
  return shallowHosts.has(parse(repo).host)
}

const defaultTarget = (repo, /* istanbul ignore next */ cwd = process.cwd()) =>
  resolve(cwd, basename(repo.replace(/[/\\]?\.git$/, '')))

const clone = (repo, revs, ref, revDoc, target, opts) => {
  if (!revDoc) {
    return unresolved(repo, ref, target, opts)
  }
  if (revDoc.sha === revs.refs.HEAD.sha) {
    return plain(repo, revDoc, target, opts)
  }
  if (revDoc.type === 'tag' || revDoc.type === 'branch') {
    return branch(repo, revDoc, target, opts)
  }
  return other(repo, revDoc, target, opts)
}

const resolveRef = (revs, ref, opts) => {
  const { spec = {} } = opts
  ref = spec.gitCommittish || ref
  /* istanbul ignore next - will fail anyway, can't pull */
  if (!revs) {
    return null
  }
  if (spec.gitRange) {
    return pickManifest(revs, spec.gitRange, opts)
  }
  if (!ref) {
    return revs.refs.HEAD
  }
  if (revs.refs[ref]) {
    return revs.refs[ref]
  }
  if (revs.shas[ref]) {
    return revs.refs[revs.shas[ref][0]]
  }
  return null
}

// pull request or some other kind of advertised ref
const other = (repo, revDoc, target, opts) => {
  const shallow = maybeShallow(repo, opts)

  const fetchOrigin = ['fetch', 'origin', revDoc.rawRef]
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
  if (maybeShallow(repo, opts)) {
    args.push('--depth=1')
  }
  if (isWindows(opts)) {
    args.push('--config', 'core.longpaths=true')
  }
  return spawn(args, opts).then(() => revDoc.sha)
}

// just the head.  clone it
const plain = (repo, revDoc, target, opts) => {
  const args = [
    'clone',
    repo,
    target,
    '--recurse-submodules',
  ]
  if (maybeShallow(repo, opts)) {
    args.push('--depth=1')
  }
  if (isWindows(opts)) {
    args.push('--config', 'core.longpaths=true')
  }
  return spawn(args, opts).then(() => revDoc.sha)
}

const updateSubmodules = (target, opts) => new Promise(resolve =>
  fs.stat(target + '/.gitmodules', er => {
    if (er) {
      return resolve(null)
    }
    return resolve(spawn([
      'submodule',
      'update',
      '-q',
      '--init',
      '--recursive',
    ], { ...opts, cwd: target }))
  }))

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
    .then(({ stdout }) => stdout.trim())
}
