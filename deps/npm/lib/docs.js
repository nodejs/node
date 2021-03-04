const log = require('npmlog')
const pacote = require('pacote')
const openUrl = require('./utils/open-url.js')
const usageUtil = require('./utils/usage.js')
const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')

class Docs {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('docs', 'npm docs [<pkgname> [<pkgname> ...]]')
  }

  exec (args, cb) {
    this.docs(args).then(() => cb()).catch(cb)
  }

  async docs (args) {
    if (!args || !args.length)
      args = ['.']

    await Promise.all(args.map(pkg => this.getDocs(pkg)))
  }

  async getDocs (pkg) {
    const opts = { ...this.npm.flatOptions, fullMetadata: true }
    const mani = await pacote.manifest(pkg, opts)
    const url = this.getDocsUrl(mani)
    log.silly('docs', 'url', url)
    await openUrl(this.npm, url, `${mani.name} docs available at the following URL`)
  }

  getDocsUrl (mani) {
    if (mani.homepage)
      return mani.homepage

    const info = hostedFromMani(mani)
    if (info)
      return info.docs()

    return 'https://www.npmjs.com/package/' + mani.name
  }
}
module.exports = Docs
