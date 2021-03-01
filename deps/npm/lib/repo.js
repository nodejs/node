const log = require('npmlog')
const pacote = require('pacote')
const { promisify } = require('util')
const openUrl = promisify(require('./utils/open-url.js'))
const usageUtil = require('./utils/usage.js')
const npm = require('./npm.js')
const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')
const { URL } = require('url')

const usage = usageUtil('repo', 'npm repo [<pkgname> [<pkgname> ...]]')

const cmd = (args, cb) => repo(args).then(() => cb()).catch(cb)

const repo = async args => {
  if (!args || !args.length)
    args = ['.']

  await Promise.all(args.map(pkg => getRepo(pkg)))
}

const getRepo = async pkg => {
  const opts = { ...npm.flatOptions, fullMetadata: true }
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
  await openUrl(url, `${mani.name} repo available at the following URL`)
}

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

module.exports = Object.assign(cmd, { usage })
