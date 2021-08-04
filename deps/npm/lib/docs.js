const log = require('npmlog')
const pacote = require('pacote')
const openUrl = require('./utils/open-url.js')
const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')

const BaseCommand = require('./base-command.js')
class Docs extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Open documentation for a package in a web browser'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'docs'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['browser', 'registry', 'workspace', 'workspaces']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<pkgname> [<pkgname> ...]]']
  }

  exec (args, cb) {
    this.docs(args).then(() => cb()).catch(cb)
  }

  execWorkspaces (args, filters, cb) {
    this.docsWorkspaces(args, filters).then(() => cb()).catch(cb)
  }

  async docs (args) {
    if (!args || !args.length)
      args = ['.']

    await Promise.all(args.map(pkg => this.getDocs(pkg)))
  }

  async docsWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    return this.docs(this.workspacePaths)
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
