const Fetcher = require('./fetcher.js')
const FileFetcher = require('./file.js')
const cacache = require('cacache')
const Minipass = require('minipass')
const { promisify } = require('util')
const readPackageJson = require('read-package-json-fast')
const isPackageBin = require('./util/is-package-bin.js')
const packlist = require('npm-packlist')
const tar = require('tar')
const _prepareDir = Symbol('_prepareDir')
const _tarcOpts = Symbol('_tarcOpts')
const { resolve } = require('path')

const runScript = require('@npmcli/run-script')

const _tarballFromResolved = Symbol.for('pacote.Fetcher._tarballFromResolved')
class DirFetcher extends Fetcher {
  constructor (spec, opts) {
    super(spec, opts)
    // just the fully resolved filename
    this.resolved = this.spec.fetchSpec
  }

  get types () {
    return ['directory']
  }

  [_prepareDir] () {
    return this.manifest().then(mani => {
      if (!mani.scripts || !mani.scripts.prepare)
        return

      // we *only* run prepare.
      // pre/post-pack is run by the npm CLI for publish and pack,
      // but this function is *also* run when installing git deps
      return runScript({
        pkg: mani,
        event: 'prepare',
        path: this.resolved,
        stdioString: true,
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
      .then(files => tar.c(this[_tarcOpts](), files)
        .on('error', er => stream.emit('error', er)).pipe(stream))
      .catch(er => stream.emit('error', er))
    return stream
  }

  [_tarcOpts] () {
    return {
      cwd: this.resolved,
      prefix: 'package/',
      portable: true,
      gzip: true,

      // ensure that package bins are always executable
      // Note that npm-packlist is already filtering out
      // anything that is not a regular file, ignored by
      // .npmignore or package.json "files", etc.
      filter: (path, stat) => {
        if (isPackageBin(this.package, path))
          stat.mode |= 0o111
        return true
      },

      // Provide a specific date in the 1980s for the benefit of zip,
      // which is confounded by files dated at the Unix epoch 0.
      mtime: new Date('1985-10-26T08:15:00.000Z'),
    }
  }

  manifest () {
    if (this.package)
      return Promise.resolve(this.package)

    return readPackageJson(this.resolved + '/package.json')
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
