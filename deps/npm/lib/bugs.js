const log = require('npmlog')
const pacote = require('pacote')
const openUrl = require('./utils/open-url.js')
const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')
const BaseCommand = require('./base-command.js')

class Bugs extends BaseCommand {
  static get description () {
    return 'Report bugs for a package in a web browser'
  }

  static get name () {
    return 'bugs'
  }

  static get usage () {
    return ['[<pkgname>]']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['browser', 'registry']
  }

  exec (args, cb) {
    this.bugs(args).then(() => cb()).catch(cb)
  }

  async bugs (args) {
    if (!args || !args.length)
      args = ['.']

    await Promise.all(args.map(pkg => this.getBugs(pkg)))
  }

  async getBugs (pkg) {
    const opts = { ...this.npm.flatOptions, fullMetadata: true }
    const mani = await pacote.manifest(pkg, opts)
    const url = this.getBugsUrl(mani)
    log.silly('bugs', 'url', url)
    await openUrl(this.npm, url, `${mani.name} bug list available at the following URL`)
  }

  getBugsUrl (mani) {
    if (mani.bugs) {
      if (typeof mani.bugs === 'string')
        return mani.bugs

      if (typeof mani.bugs === 'object' && mani.bugs.url)
        return mani.bugs.url

      if (typeof mani.bugs === 'object' && mani.bugs.email)
        return `mailto:${mani.bugs.email}`
    }

    // try to get it from the repo, if possible
    const info = hostedFromMani(mani)
    if (info)
      return info.bugs()

    // just send them to the website, hopefully that has some info!
    return `https://www.npmjs.com/package/${mani.name}`
  }
}

module.exports = Bugs
