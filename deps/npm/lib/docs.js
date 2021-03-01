const log = require('npmlog')
const pacote = require('pacote')
const { promisify } = require('util')
const openUrl = promisify(require('./utils/open-url.js'))
const usageUtil = require('./utils/usage.js')
const npm = require('./npm.js')
const hostedFromMani = require('./utils/hosted-git-info-from-manifest.js')

const usage = usageUtil('docs', 'npm docs [<pkgname> [<pkgname> ...]]')

const cmd = (args, cb) => docs(args).then(() => cb()).catch(cb)

const docs = async args => {
  if (!args || !args.length)
    args = ['.']

  await Promise.all(args.map(pkg => getDocs(pkg)))
}

const getDocsUrl = mani => {
  if (mani.homepage)
    return mani.homepage

  const info = hostedFromMani(mani)
  if (info)
    return info.docs()

  return 'https://www.npmjs.com/package/' + mani.name
}

const getDocs = async pkg => {
  const opts = { ...npm.flatOptions, fullMetadata: true }
  const mani = await pacote.manifest(pkg, opts)
  const url = getDocsUrl(mani)
  log.silly('docs', 'url', url)
  await openUrl(url, `${mani.name} docs available at the following URL`)
}

module.exports = Object.assign(cmd, { usage })
