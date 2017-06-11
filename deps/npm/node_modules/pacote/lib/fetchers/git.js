'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const cacheKey = require('../util/cache-key')
const Fetcher = require('../fetch')
const git = require('../util/git')
const mkdirp = BB.promisify(require('mkdirp'))
const pickManifest = require('npm-pick-manifest')
const optCheck = require('../util/opt-check')
const osenv = require('osenv')
const packDir = require('../util/pack-dir')
const PassThrough = require('stream').PassThrough
const path = require('path')
const pipe = BB.promisify(require('mississippi').pipe)
const rimraf = BB.promisify(require('rimraf'))
const uniqueFilename = require('unique-filename')

// `git` dependencies are fetched from git repositories and packed up.
const fetchGit = module.exports = Object.create(null)

Fetcher.impl(fetchGit, {
  manifest (spec, opts) {
    opts = optCheck(opts)
    if (spec.hosted && spec.hosted.getDefaultRepresentation() === 'shortcut') {
      return hostedManifest(spec, opts)
    } else {
      // If it's not a shortcut, don't do fallbacks.
      return plainManifest(spec.fetchSpec, spec, opts)
    }
  },

  tarball (spec, opts) {
    opts = optCheck(opts)
    const stream = new PassThrough()
    this.manifest(spec, opts).then(manifest => {
      stream.emit('manifest', manifest)
      return pipe(
        this.fromManifest(
          manifest, spec, opts
        ).on('integrity', i => stream.emit('integrity', i)), stream
      )
    }, err => stream.emit('error', err))
    return stream
  },

  fromManifest (manifest, spec, opts) {
    opts = optCheck(opts)
    let streamError
    const stream = new PassThrough().on('error', e => { streamError = e })
    const cacheName = manifest._uniqueResolved || manifest._resolved || ''
    const cacheStream = (
      opts.cache &&
      cacache.get.stream(
        opts.cache, cacheKey('packed-dir', cacheName), opts
      ).on('integrity', i => stream.emit('integrity', i))
    )
    cacheStream.pipe(stream)
    cacheStream.on('error', err => {
      if (err.code !== 'ENOENT') {
        return stream.emit('error', err)
      } else {
        stream.emit('reset')
        return withTmp(opts, tmp => {
          if (streamError) { throw streamError }
          return cloneRepo(
            manifest._repo, manifest._ref, manifest._rawRef, tmp, opts
          ).then(HEAD => {
            if (streamError) { throw streamError }
            manifest._resolved = spec.saveSpec.replace(/(:?#.*)?$/, `#${HEAD}`)
            manifest._uniqueResolved = manifest._resolved
            return packDir(manifest, manifest._uniqueResolved, tmp, stream, opts)
          })
        }).catch(err => stream.emit('error', err))
      }
    })
    return stream
  }
})

function hostedManifest (spec, opts) {
  return BB.resolve(null).then(() => {
    if (!spec.hosted.git()) {
      throw new Error(`No git url for ${spec}`)
    }
    return plainManifest(spec.hosted.git(), spec, opts)
  }).catch(err => {
    if (!spec.hosted.https()) {
      throw err
    }
    return plainManifest(spec.hosted.https(), spec, opts)
  }).catch(err => {
    if (!spec.hosted.sshurl()) {
      throw err
    }
    return plainManifest(spec.hosted.sshurl(), spec, opts)
  })
}

function plainManifest (repo, spec, opts) {
  const rawRef = spec.gitCommittish || spec.gitRange
  return resolve(
    repo, spec, spec.name, opts
  ).then(ref => {
    if (ref) {
      const resolved = spec.saveSpec.replace(/(?:#.*)?$/, `#${ref.sha}`)
      return {
        _repo: repo,
        _resolved: resolved,
        _spec: spec,
        _ref: ref,
        _rawRef: spec.gitCommittish || spec.gitRange,
        _uniqueResolved: resolved
      }
    } else {
      // We're SOL and need a full clone :(
      //
      // If we're confident enough that `rawRef` is a commit SHA,
      // then we can at least get `finalize-manifest` to cache its result.
      const resolved = spec.saveSpec.replace(/(?:#.*)?$/, `#${rawRef}`)
      return {
        _repo: repo,
        _rawRef: rawRef,
        _resolved: rawRef.match(/^[a-f0-9]{40}$/) && resolved,
        _uniqueResolved: rawRef.match(/^[a-f0-9]{40}$/) && resolved
      }
    }
  })
}

function resolve (url, spec, name, opts) {
  const isSemver = !!spec.gitRange
  return git.revs(url, opts).then(remoteRefs => {
    return isSemver
    ? pickManifest({
      versions: remoteRefs.versions,
      'dist-tags': remoteRefs['dist-tags'],
      name: name
    }, spec.gitRange, opts)
    : remoteRefs
    ? BB.resolve(
      remoteRefs.refs[spec.gitCommittish] || remoteRefs.refs[remoteRefs.shas[spec.gitCommittish]]
    )
    : null
  })
}

function withTmp (opts, cb) {
  if (opts.cache) {
    // cacache has a special facility for working in a tmp dir
    return cacache.tmp.withTmp(opts.cache, {tmpPrefix: 'git-clone'}, cb)
  } else {
    const tmpDir = path.join(osenv.tmpdir(), 'pacote-git-tmp')
    const tmpName = uniqueFilename(tmpDir, 'git-clone')
    const tmp = mkdirp(tmpName).then(() => tmpName).disposer(rimraf)
    return BB.using(tmp, cb)
  }
}

function cloneRepo (repo, resolvedRef, rawRef, tmp, opts) {
  if (resolvedRef) {
    return git.shallow(repo, resolvedRef.ref, tmp, opts)
  } else {
    return git.clone(repo, rawRef, tmp, opts)
  }
}
