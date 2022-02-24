const Fetcher = require('./fetcher.js')
const FileFetcher = require('./file.js')
const Minipass = require('minipass')
const tarCreateOptions = require('./util/tar-create-options.js')
const packlist = require('npm-packlist')
const tar = require('tar')
const _prepareDir = Symbol('_prepareDir')
const { resolve } = require('path')
const _readPackageJson = Symbol.for('package.Fetcher._readPackageJson')

const runScript = require('@npmcli/run-script')

const _tarballFromResolved = Symbol.for('pacote.Fetcher._tarballFromResolved')
class DirFetcher extends Fetcher {
  constructor (spec, opts) {
    super(spec, opts)
    // just the fully resolved filename
    this.resolved = this.spec.fetchSpec
  }

  // exposes tarCreateOptions as public API
  static tarCreateOptions (manifest) {
    return tarCreateOptions(manifest)
  }

  get types () {
    return ['directory']
  }

  [_prepareDir] () {
    return this.manifest().then(mani => {
      if (!mani.scripts || !mani.scripts.prepare) {
        return
      }

      // we *only* run prepare.
      // pre/post-pack is run by the npm CLI for publish and pack,
      // but this function is *also* run when installing git deps
      const stdio = this.opts.foregroundScripts ? 'inherit' : 'pipe'

      // hide the banner if silent opt is passed in, or if prepare running
      // in the background.
      const banner = this.opts.silent ? false : stdio === 'inherit'

      return runScript({
        pkg: mani,
        event: 'prepare',
        path: this.resolved,
        stdioString: true,
        stdio,
        banner,
        env: {
          npm_package_resolved: this.resolved,
          npm_package_integrity: this.integrity,
          npm_package_json: resolve(this.resolved, 'package.json'),
        },
      })
    })
  }

  [_tarballFromResolved] () {
    const stream = new Minipass()
    stream.resolved = this.resolved
    stream.integrity = this.integrity

    // run the prepare script, get the list of files, and tar it up
    // pipe to the stream, and proxy errors the chain.
    this[_prepareDir]()
      .then(() => packlist({ path: this.resolved }))
      .then(files => tar.c(tarCreateOptions(this.package), files)
        .on('error', er => stream.emit('error', er)).pipe(stream))
      .catch(er => stream.emit('error', er))
    return stream
  }

  manifest () {
    if (this.package) {
      return Promise.resolve(this.package)
    }

    return this[_readPackageJson](this.resolved + '/package.json')
      .then(mani => this.package = {
        ...mani,
        _integrity: this.integrity && String(this.integrity),
        _resolved: this.resolved,
        _from: this.from,
      })
  }

  packument () {
    return FileFetcher.prototype.packument.apply(this)
  }
}
module.exports = DirFetcher
