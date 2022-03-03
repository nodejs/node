const pacote = require('pacote')
const openUrl = require('../utils/open-url.js')
const hostedFromMani = require('../utils/hosted-git-info-from-manifest.js')
const log = require('../utils/log-shim')
const BaseCommand = require('../base-command.js')
class Docs extends BaseCommand {
  static description = 'Open documentation for a package in a web browser'
  static name = 'docs'
  static params = [
    'browser',
    'registry',
    'workspace',
    'workspaces',
    'include-workspace-root',
  ]

  static usage = ['[<pkgname> [<pkgname> ...]]']
  static ignoreImplicitWorkspace = false

  async exec (args) {
    if (!args || !args.length) {
      args = ['.']
    }

    await Promise.all(args.map(pkg => this.getDocs(pkg)))
  }

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    return this.exec(this.workspacePaths)
  }

  async getDocs (pkg) {
    const opts = { ...this.npm.flatOptions, fullMetadata: true }
    const mani = await pacote.manifest(pkg, opts)
    const url = this.getDocsUrl(mani)
    log.silly('docs', 'url', url)
    await openUrl(this.npm, url, `${mani.name} docs available at the following URL`)
  }

  getDocsUrl (mani) {
    if (mani.homepage) {
      return mani.homepage
    }

    const info = hostedFromMani(mani)
    if (info) {
      return info.docs()
    }

    return 'https://www.npmjs.com/package/' + mani.name
  }
}
module.exports = Docs
