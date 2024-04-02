const Fetcher = require('./fetcher.js')
const fsm = require('fs-minipass')
const cacache = require('cacache')
const _tarballFromResolved = Symbol.for('pacote.Fetcher._tarballFromResolved')
const _exeBins = Symbol('_exeBins')
const { resolve } = require('path')
const fs = require('fs')
const _readPackageJson = Symbol.for('package.Fetcher._readPackageJson')

class FileFetcher extends Fetcher {
  constructor (spec, opts) {
    super(spec, opts)
    // just the fully resolved filename
    this.resolved = this.spec.fetchSpec
  }

  get types () {
    return ['file']
  }

  manifest () {
    if (this.package) {
      return Promise.resolve(this.package)
    }

    // have to unpack the tarball for this.
    return cacache.tmp.withTmp(this.cache, this.opts, dir =>
      this.extract(dir)
        .then(() => this[_readPackageJson](dir + '/package.json'))
        .then(mani => this.package = {
          ...mani,
          _integrity: this.integrity && String(this.integrity),
          _resolved: this.resolved,
          _from: this.from,
        }))
  }

  [_exeBins] (pkg, dest) {
    if (!pkg.bin) {
      return Promise.resolve()
    }

    return Promise.all(Object.keys(pkg.bin).map(k => new Promise(res => {
      const script = resolve(dest, pkg.bin[k])
      // Best effort.  Ignore errors here, the only result is that
      // a bin script is not executable.  But if it's missing or
      // something, we just leave it for a later stage to trip over
      // when we can provide a more useful contextual error.
      fs.stat(script, (er, st) => {
        if (er) {
          return res()
        }
        const mode = st.mode | 0o111
        if (mode === st.mode) {
          return res()
        }
        fs.chmod(script, mode, res)
      })
    })))
  }

  extract (dest) {
    // if we've already loaded the manifest, then the super got it.
    // but if not, read the unpacked manifest and chmod properly.
    return super.extract(dest)
      .then(result => this.package ? result
      : this[_readPackageJson](dest + '/package.json').then(pkg =>
        this[_exeBins](pkg, dest)).then(() => result))
  }

  [_tarballFromResolved] () {
    // create a read stream and return it
    return new fsm.ReadStream(this.resolved)
  }

  packument () {
    // simulate based on manifest
    return this.manifest().then(mani => ({
      name: mani.name,
      'dist-tags': {
        [this.defaultTag]: mani.version,
      },
      versions: {
        [mani.version]: {
          ...mani,
          dist: {
            tarball: `file:${this.resolved}`,
            integrity: this.integrity && String(this.integrity),
          },
        },
      },
    }))
  }
}

module.exports = FileFetcher
