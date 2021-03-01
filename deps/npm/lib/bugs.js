const log = require('npmlog')
const pacote = require('pacote')
const { promisify } = require('util')
const openUrl = promisify(require('./utils/open-url.js'))
const usageUtil = require('./utils/usage.js')
const npm = require('./npm.js')
const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')

const usage = usageUtil('bugs', 'npm bugs [<pkgname>]')

const cmd = (args, cb) => bugs(args).then(() => cb()).catch(cb)

const bugs = async args => {
  if (!args || !args.length)
    args = ['.']

  await Promise.all(args.map(pkg => getBugs(pkg)))
}

const getBugsUrl = mani => {
  if (mani.bugs) {
    if (typeof mani.bugs === 'string')
      return mani.bugs

    if (typeof mani.bugs === 'object' && mani.bugs.url)
      return mani.bugs.url
  }

  // try to get it from the repo, if possible
  const info = hostedFromMani(mani)
  if (info)
    return info.bugs()

  // just send them to the website, hopefully that has some info!
  return `https://www.npmjs.com/package/${mani.name}`
}

const getBugs = async pkg => {
  const opts = { ...npm.flatOptions, fullMetadata: true }
  const mani = await pacote.manifest(pkg, opts)
  const url = getBugsUrl(mani)
  log.silly('bugs', 'url', url)
  await openUrl(url, `${mani.name} bug list available at the following URL`)
}

module.exports = Object.assign(cmd, { usage })
