const { resolve, join } = require('node:path')
const { mkdtemp, writeFile, rm } = require('node:fs/promises')
const { tmpdir } = require('node:os')
const packlist = require('npm-packlist')
const runScript = require('@npmcli/run-script')
const tar = require('tar')
const { Minipass } = require('minipass')
const PackageJson = require('@npmcli/package-json')
const Fetcher = require('./fetcher.js')
const FileFetcher = require('./file.js')
const _ = require('./util/protected.js')
const tarCreateOptions = require('./util/tar-create-options.js')

class DirFetcher extends Fetcher {
  constructor (spec, opts) {
    super(spec, opts)
    // just the fully resolved filename
    this.resolved = this.spec.fetchSpec

    this.tree = opts.tree || null
    this.Arborist = opts.Arborist || null
  }

  // exposes tarCreateOptions as public API
  static tarCreateOptions (manifest) {
    return tarCreateOptions(manifest)
  }

  get types () {
    return ['directory']
  }

  #prepareDir () {
    return this.manifest().then(mani => {
      if (!mani.scripts || !mani.scripts.prepare) {
        return
      }
      if (this.opts.ignoreScripts) {
        return
      }

      // we *only* run prepare.
      // pre/post-pack is run by the npm CLI for publish and pack,
      // but this function is *also* run when installing git deps
      const stdio = this.opts.foregroundScripts ? 'inherit' : 'pipe'

      return runScript({
        // this || undefined is because runScript will be unhappy with the default null value
        scriptShell: this.opts.scriptShell || undefined,
        pkg: mani,
        event: 'prepare',
        path: this.resolved,
        stdio,
        env: {
          npm_package_resolved: this.resolved,
          npm_package_integrity: this.integrity,
          npm_package_json: resolve(this.resolved, 'package.json'),
        },
      })
    })
  }

  [_.tarballFromResolved] () {
    if (!this.tree && !this.Arborist) {
      throw new Error('DirFetcher requires either a tree or an Arborist constructor to pack')
    }

    const stream = new Minipass()
    stream.resolved = this.resolved
    stream.integrity = this.integrity

    const { prefix, workspaces, globalIgnoreFile } = this.opts

    // run the prepare script, get the list of files, and tar it up
    // pipe to the stream, and proxy errors the chain.
    this.#prepareDir()
      .then(async () => {
        if (!this.tree) {
          const arb = new this.Arborist({ path: this.resolved })
          this.tree = await arb.loadActual()
        }
        return packlist(this.tree, { path: this.resolved, prefix, workspaces, globalIgnoreFile })
      })
      .then(async files => {
        const { options, cleanup } = await this.#tarOptions()
        const source = tar.c(options, files)
        // the strip temp file must outlive content consumption, so clean up once the stream is done
        source.once('end', cleanup)
        source.once('error', cleanup)
        return source.on('error', er => stream.emit('error', er)).pipe(stream)
      })
      .catch(er => stream.emit('error', er))
    return stream
  }

  // Build the tar create options.
  // When the packed package.json declares patchedDependencies, redirect it to a stripped copy so project-local patches never ship.
  // Non-patched packs are unchanged.
  async #tarOptions () {
    const options = tarCreateOptions(this.package)

    // read package.json from disk after prepare so the strip reflects the actually-packed manifest.
    const pkgJson = await PackageJson.load(this.resolved)
    if (!('patchedDependencies' in pkgJson.content)) {
      return { options, cleanup: () => {} }
    }

    // serialize the package.json minus patchedDependencies, preserving its indent and newline.
    // JSON.stringify ignores the indent and newline symbols @npmcli/package-json attaches to content.
    delete pkgJson.content.patchedDependencies
    const { content } = pkgJson
    const indent = content[Symbol.for('indent')]
    const newline = content[Symbol.for('newline')]
    const stripped = `${JSON.stringify(content, null, indent)}\n`.replace(/\n/g, newline)

    // write the stripped copy to a temp dir, removing it if the write itself fails.
    const dir = await mkdtemp(join(tmpdir(), 'pacote-pack-'))
    const strippedPath = join(dir, 'package.json')
    try {
      await writeFile(strippedPath, stripped)
    } catch (er) {
      /* istanbul ignore next - writing to a freshly created temp dir is not deterministically failable */
      await rm(dir, { recursive: true, force: true })
      /* istanbul ignore next */
      throw er
    }
    const size = Buffer.byteLength(stripped)

    // point only the top-level package.json entry at the stripped copy; every other file is untouched.
    // onWriteEntry runs before the tar header and the file's hardlink check, so size and nlink here are honored.
    options.onWriteEntry = (entry) => {
      if (entry.path === 'package.json') {
        entry.absolute = strippedPath
        entry.stat.size = size
        entry.stat.nlink = 1
      }
    }

    return { options, cleanup: () => rm(dir, { recursive: true, force: true }) }
  }

  manifest () {
    if (this.package) {
      return Promise.resolve(this.package)
    }

    return this[_.readPackageJson](this.resolved)
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
