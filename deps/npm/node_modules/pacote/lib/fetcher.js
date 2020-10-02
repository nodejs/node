// This is the base class that the other fetcher types in lib
// all descend from.
// It handles the unpacking and retry logic that is shared among
// all of the other Fetcher types.

const npa = require('npm-package-arg')
const ssri = require('ssri')
const { promisify } = require('util')
const { basename, dirname } = require('path')
const rimraf = promisify(require('rimraf'))
const tar = require('tar')
const procLog = require('./util/proc-log.js')
const retry = require('promise-retry')
const fsm = require('fs-minipass')
const cacache = require('cacache')
const isPackageBin = require('./util/is-package-bin.js')
const getContents = require('@npmcli/installed-package-contents')

// we only change ownership on unix platforms, and only if uid is 0
const selfOwner = process.getuid && process.getuid() === 0 ? {
  uid: 0,
  gid: process.getgid(),
} : null
const chownr = selfOwner ? promisify(require('chownr')) : null
const inferOwner = selfOwner ? require('infer-owner') : null
const mkdirp = require('mkdirp')
const cacheDir = require('./util/cache-dir.js')

// Private methods.
// Child classes should not have to override these.
// Users should never call them.
const _chown = Symbol('_chown')
const _extract = Symbol('_extract')
const _mkdir = Symbol('_mkdir')
const _empty = Symbol('_empty')
const _toFile = Symbol('_toFile')
const _tarxOptions = Symbol('_tarxOptions')
const _entryMode = Symbol('_entryMode')
const _istream = Symbol('_istream')
const _assertType = Symbol('_assertType')
const _tarballFromCache = Symbol('_tarballFromCache')
const _tarballFromResolved = Symbol.for('pacote.Fetcher._tarballFromResolved')

class FetcherBase {
  constructor (spec, opts) {
    if (!opts || typeof opts !== 'object')
      throw new TypeError('options object is required')
    this.spec = npa(spec, opts.where)

    // a bit redundant because presumably the caller already knows this,
    // but it makes it easier to not have to keep track of the requested
    // spec when we're dispatching thousands of these at once, and normalizing
    // is nice.  saveSpec is preferred if set, because it turns stuff like
    // x/y#committish into github:x/y#committish.  use name@rawSpec for
    // registry deps so that we turn xyz and xyz@ -> xyz@
    this.from = this.spec.registry
      ? `${this.spec.name}@${this.spec.rawSpec}` : this.spec.saveSpec

    this[_assertType]()
    // clone the opts object so that others aren't upset when we mutate it
    // by adding/modifying the integrity value.
    this.opts = {...opts}
    this.cache = opts.cache || cacheDir()
    this.resolved = opts.resolved || null

    // default to caching/verifying with sha512, that's what we usually have
    // need to change this default, or start overriding it, when sha512
    // is no longer strong enough.
    this.defaultIntegrityAlgorithm = opts.defaultIntegrityAlgorithm || 'sha512'

    if (typeof opts.integrity === 'string')
      this.opts.integrity = ssri.parse(opts.integrity)

    this.package = null
    this.type = this.constructor.name
    this.fmode = opts.fmode || 0o666
    this.dmode = opts.dmode || 0o777
    this.umask = opts.umask || 0o022
    this.log = opts.log || procLog

    this.preferOnline = !!opts.preferOnline
    this.preferOffline = !!opts.preferOffline
    this.offline = !!opts.offline

    this.before = opts.before
    this.fullMetadata = this.before ? true : !!opts.fullMetadata

    this.defaultTag = opts.defaultTag || 'latest'
    this.registry = (opts.registry || 'https://registry.npmjs.org')
      .replace(/\/+$/, '')

    // command to run 'prepare' scripts on directories and git dirs
    // To use pacote with yarn, for example, set npmBin to 'yarn'
    // and npmInstallCmd to ['add'], and npmCliConfig with yarn's equivalents.
    this.npmBin = opts.npmBin || 'npm'

    // command to install deps for preparing
    this.npmInstallCmd = opts.npmInstallCmd || [
      'install',
      '--only=dev',
      '--prod',
      '--ignore-prepublish',
      '--no-progress',
      '--no-save',
    ]

    // XXX fill more of this in based on what we know from this.opts
    // we explicitly DO NOT fill in --tag, though, since we are often
    // going to be packing in the context of a publish, which may set
    // a dist-tag, but certainly wants to keep defaulting to latest.
    this.npmCliConfig = opts.npmCliConfig || [
      `--cache=${this.cache}`,
      `--prefer-offline=${!!this.preferOffline}`,
      `--prefer-online=${!!this.preferOnline}`,
      `--offline=${!!this.offline}`,
      `--before=${this.before ? this.before.toISOString() : ''}`,
    ]
  }

  get integrity () {
    return this.opts.integrity || null
  }
  set integrity (i) {
    if (!i)
      return

    i = ssri.parse(i)
    const current = this.opts.integrity

    // do not ever update an existing hash value, but do
    // merge in NEW algos and hashes that we don't already have.
    if (current)
      current.merge(i)
    else
      this.opts.integrity = i
  }

  get notImplementedError () {
    return new Error('not implemented in this fetcher type: ' + this.type)
  }

  // override in child classes
  // Returns a Promise that resolves to this.resolved string value
  resolve () {
    return this.resolved ? Promise.resolve(this.resolved)
      : Promise.reject(this.notImplementedError)
  }

  packument () {
    return Promise.reject(this.notImplementedError)
  }

  // override in child class
  // returns a manifest containing:
  // - name
  // - version
  // - _resolved
  // - _integrity
  // - plus whatever else was in there (corgi, full metadata, or pj file)
  manifest () {
    return Promise.reject(this.notImplementedError)
  }

  // private, should be overridden.
  // Note that they should *not* calculate or check integrity, but *just*
  // return the raw tarball data stream.
  [_tarballFromResolved] () {
    throw this.notImplementedError
  }

  // public, should not be overridden
  tarball () {
    return this.tarballStream(stream => new Promise((res, rej) => {
      const buf = []
      stream.on('error', er => rej(er))
      stream.on('end', () => {
        const data = Buffer.concat(buf)
        data.integrity = this.integrity && String(this.integrity)
        data.resolved = this.resolved
        data.from = this.from
        return res(data)
      })
      stream.on('data', d => buf.push(d))
    }))
  }

  // private
  // Note: cacache will raise a EINTEGRITY error if the integrity doesn't match
  [_tarballFromCache] () {
    return cacache.get.stream.byDigest(this.cache, this.integrity, this.opts)
  }

  [_istream] (stream) {
    // everyone will need one of these, either for verifying or calculating
    // We always set it, because we have might only have a weak legacy hex
    // sha1 in the packument, and this MAY upgrade it to a stronger algo.
    // If we had an integrity, and it doesn't match, then this does not
    // override that error; the istream will raise the error before it
    // gets to the point of re-setting the integrity.
    const istream = ssri.integrityStream(this.opts)
    istream.on('integrity', i => this.integrity = i)
    return stream.on('error', er => istream.emit('error', er)).pipe(istream)
  }

  pickIntegrityAlgorithm () {
    return this.integrity ? this.integrity.pickAlgorithm(this.opts)
      : this.defaultIntegrityAlgorithm
  }

  // TODO: check error class, once those are rolled out to our deps
  isDataCorruptionError (er) {
    return er.code === 'EINTEGRITY' || er.code === 'Z_DATA_ERROR'
  }

  // override the types getter
  get types () {}
  [_assertType] () {
    if (this.types && !this.types.includes(this.spec.type)) {
      throw new TypeError(`Wrong spec type (${
        this.spec.type
      }) for ${
        this.constructor.name
      }. Supported types: ${this.types.join(', ')}`)
    }
  }

  // We allow ENOENTs from cacache, but not anywhere else.
  // An ENOENT trying to read a tgz file, for example, is Right Out.
  isRetriableError (er) {
    // TODO: check error class, once those are rolled out to our deps
    return this.isDataCorruptionError(er) || er.code === 'ENOENT'
  }

  // Mostly internal, but has some uses
  // Pass in a function which returns a promise
  // Function will be called 1 or more times with streams that may fail.
  // Retries:
  // Function MUST handle errors on the stream by rejecting the promise,
  // so that retry logic can pick it up and either retry or fail whatever
  // promise it was making (ie, failing extraction, etc.)
  //
  // The return value of this method is a Promise that resolves the same
  // as whatever the streamHandler resolves to.
  //
  // This should never be overridden by child classes, but it is public.
  tarballStream (streamHandler) {
    // Only short-circuit via cache if we have everything else we'll need,
    // and the user has not expressed a preference for checking online.

    const fromCache = (
      !this.preferOnline &&
      this.integrity &&
      this.resolved
    ) ? streamHandler(this[_tarballFromCache]()).catch(er => {
      if (this.isDataCorruptionError(er)) {
        this.log.warn('tarball', `cached data for ${
          this.spec
        } (${this.integrity}) seems to be corrupted. Refreshing cache.`)
        return this.cleanupCached().then(() => { throw er })
      } else {
        throw er
      }
    }) : null

    const fromResolved = er => {
      if (er) {
        if (!this.isRetriableError(er))
          throw er
        this.log.silly('tarball', `no local data for ${
          this.spec
        }. Extracting by manifest.`)
      }
      return this.resolve().then(() => retry(tryAgain =>
        streamHandler(this[_istream](this[_tarballFromResolved]()))
        .catch(er => {
          // Most likely data integrity.  A cache ENOENT error is unlikely
          // here, since we're definitely not reading from the cache, but it
          // IS possible that the fetch subsystem accessed the cache, and the
          // entry got blown away or something.  Try one more time to be sure.
          if (this.isRetriableError(er)) {
            this.log.warn('tarball', `tarball data for ${
              this.spec
            } (${this.integrity}) seems to be corrupted. Trying again.`)
            return this.cleanupCached().then(() => tryAgain(er))
          }
          throw er
        }), { retries: 1, minTimeout: 0, maxTimeout: 0 }))
    }

    return fromCache ? fromCache.catch(fromResolved) : fromResolved()
  }

  cleanupCached () {
    return cacache.rm.content(this.cache, this.integrity, this.opts)
  }

  [_chown] (path, uid, gid) {
    return selfOwner && (selfOwner.gid !== gid || selfOwner.uid !== uid)
      ? chownr(path, uid, gid)
      : /* istanbul ignore next - we don't test in root-owned folders */ null
  }

  [_empty] (path) {
    return getContents({path, depth: 1}).then(contents => Promise.all(
      contents.map(entry => rimraf(entry))))
  }

  [_mkdir] (dest) {
    // if we're bothering to do owner inference, then do it.
    // otherwise just make the dir, and return an empty object.
    // always empty the dir dir to start with, but do so
    // _after_ inferring the owner, in case there's an existing folder
    // there that we would want to preserve which differs from the
    // parent folder (rare, but probably happens sometimes).
    return !inferOwner
      ? this[_empty](dest).then(() => mkdirp(dest)).then(() => ({}))
      : inferOwner(dest).then(({uid, gid}) =>
        this[_empty](dest)
          .then(() => mkdirp(dest))
          .then(made => {
            // ignore the || dest part in coverage.  It's there to handle
            // race conditions where the dir may be made by someone else
            // after being removed by us.
            const dir = made || /* istanbul ignore next */ dest
            return this[_chown](dir, uid, gid)
          })
          .then(() => ({uid, gid})))
  }

  // extraction is always the same.  the only difference is where
  // the tarball comes from.
  extract (dest) {
    return this[_mkdir](dest).then(({uid, gid}) =>
      this.tarballStream(tarball => this[_extract](dest, tarball, uid, gid)))
  }

  [_toFile] (dest) {
    return this.tarballStream(str => new Promise((res, rej) => {
      const writer = new fsm.WriteStream(dest)
      str.on('error', er => writer.emit('error', er))
      writer.on('error', er => rej(er))
      writer.on('close', () => res({
        integrity: this.integrity && String(this.integrity),
        resolved: this.resolved,
        from: this.from,
      }))
      str.pipe(writer)
    }))
  }

  // don't use this[_mkdir] because we don't want to rimraf anything
  tarballFile (dest) {
    const dir = dirname(dest)
    return !inferOwner
      ? mkdirp(dir).then(() => this[_toFile](dest))
      : inferOwner(dest).then(({uid, gid}) =>
        mkdirp(dir).then(made => this[_toFile](dest)
          .then(res => this[_chown](made || dir, uid, gid)
            .then(() => res))))
  }

  [_extract] (dest, tarball, uid, gid) {
    const extractor = tar.x(this[_tarxOptions]({ cwd: dest, uid, gid }))
    const p = new Promise((resolve, reject) => {
      extractor.on('end', () => {
        resolve({
          resolved: this.resolved,
          integrity: this.integrity && String(this.integrity),
          from: this.from,
        })
      })

      extractor.on('error', er => {
        this.log.warn('tar', er.message)
        this.log.silly('tar', er)
        reject(er)
      })

      tarball.on('error', er => reject(er))
    })

    tarball.pipe(extractor)
    return p
  }

  // always ensure that entries are at least as permissive as our configured
  // dmode/fmode, but never more permissive than the umask allows.
  [_entryMode] (path, mode, type) {
    const m = /Directory|GNUDumpDir/.test(type) ? this.dmode
      : /File$/.test(type) ? this.fmode
      : /* istanbul ignore next - should never happen in a pkg */ 0

    // make sure package bins are executable
    const exe = isPackageBin(this.package, path) ? 0o111 : 0
    return ((mode | m) & ~this.umask) | exe
  }

  [_tarxOptions] ({ cwd, uid, gid }) {
    const sawIgnores = new Set()
    return {
      cwd,
      filter: (name, entry) => {
        if (/Link$/.test(entry.type))
          return false
        entry.mode = this[_entryMode](entry.path, entry.mode, entry.type)
        // this replicates the npm pack behavior where .gitignore files
        // are treated like .npmignore files, but only if a .npmignore
        // file is not present.
        if (/File$/.test(entry.type)) {
          const base = basename(entry.path)
          if (base === '.npmignore')
            sawIgnores.add(entry.path)
          else if (base === '.gitignore') {
            // rename, but only if there's not already a .npmignore
            const ni = entry.path.replace(/\.gitignore$/, '.npmignore')
            if (sawIgnores.has(ni))
              return false
            entry.path = ni
          }
          return true
        }
      },
      strip: 1,
      onwarn: /* istanbul ignore next - we can trust that tar logs */
      (code, msg, data) => {
        this.log.warn('tar', code, msg)
        this.log.silly('tar', code, msg, data)
      },
      uid,
      gid,
      umask: this.umask,
    }
  }
}

module.exports = FetcherBase

// Child classes
const GitFetcher = require('./git.js')
const RegistryFetcher = require('./registry.js')
const FileFetcher = require('./file.js')
const DirFetcher = require('./dir.js')
const RemoteFetcher = require('./remote.js')

// Get an appropriate fetcher object from a spec and options
FetcherBase.get = (rawSpec, opts = {}) => {
  const spec = npa(rawSpec, opts.where)
  switch (spec.type) {
    case 'git':
      return new GitFetcher(spec, opts)

    case 'remote':
      return new RemoteFetcher(spec, opts)

    case 'version':
    case 'range':
    case 'tag':
    case 'alias':
      return new RegistryFetcher(spec.subSpec || spec, opts)

    case 'file':
      return new FileFetcher(spec, opts)

    case 'directory':
      return new DirFetcher(spec, opts)

    default:
      throw new TypeError('Unknown spec type: ' + spec.type)
  }
}
