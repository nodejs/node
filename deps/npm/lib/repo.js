const log = require('npmlog')
const pacote = require('pacote')
const { URL } = require('url')

const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')
const openUrl = require('./utils/open-url.js')

const BaseCommand = require('./base-command.js')
class Repo extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'repo'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<pkgname> [<pkgname> ...]]']
  }

  exec (args, cb) {
    this.repo(args).then(() => cb()).catch(cb)
  }

  async repo (args) {
    if (!args || !args.length)
      args = ['.']

    await Promise.all(args.map(pkg => this.get(pkg)))
  }

  async get (pkg) {
    const opts = { ...this.npm.flatOptions, fullMetadata: true }
    const mani = await pacote.manifest(pkg, opts)

    const r = mani.repository
    const rurl = !r ? null
      : typeof r === 'string' ? r
      : typeof r === 'object' && typeof r.url === 'string' ? r.url
      : null

    if (!rurl) {
      throw Object.assign(new Error('no repository'), {
        pkgid: pkg,
      })
    }

    const info = hostedFromMani(mani)
    const url = info ?
      info.browse(mani.repository.directory) : unknownHostedUrl(rurl)

    if (!url) {
      throw Object.assign(new Error('no repository: could not get url'), {
        pkgid: pkg,
      })
    }

    log.silly('docs', 'url', url)
    await openUrl(this.npm, url, `${mani.name} repo available at the following URL`)
  }
}
module.exports = Repo

const unknownHostedUrl = url => {
  try {
    const {
      protocol,
      hostname,
      pathname,
    } = new URL(url)

    /* istanbul ignore next - URL ctor should prevent this */
    if (!protocol || !hostname)
      return null

    const proto = /(git\+)http:$/.test(protocol) ? 'http:' : 'https:'
    const path = pathname.replace(/\.git$/, '')
    return `${proto}//${hostname}${path}`
  } catch (e) {
    return null
  }
}
