'use strict'

const path = require('path')

const archy = require('archy')
const figgyPudding = require('figgy-pudding')
const readPackageTree = require('read-package-tree')

const npm = require('./npm.js')
const npmConfig = require('./config/figgy-config.js')
const fetchPackageMetadata = require('./fetch-package-metadata.js')
const computeMetadata = require('./install/deps.js').computeMetadata
const readShrinkwrap = require('./install/read-shrinkwrap.js')
const mutateIntoLogicalTree = require('./install/mutate-into-logical-tree.js')
const output = require('./utils/output.js')
const openUrl = require('./utils/open-url.js')
const { getFundingInfo, retrieveFunding, validFundingField, flatCacheSymbol } = require('./utils/funding.js')

const FundConfig = figgyPudding({
  browser: {}, // used by ./utils/open-url
  global: {},
  json: {},
  unicode: {},
  which: {}
})

module.exports = fundCmd

const usage = require('./utils/usage')
fundCmd.usage = usage(
  'fund',
  'npm fund [--json]',
  'npm fund [--browser] [[<@scope>/]<pkg> [--which=<fundingSourceNumber>]'
)

fundCmd.completion = function (opts, cb) {
  const argv = opts.conf.argv.remain
  switch (argv[2]) {
    case 'fund':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function printJSON (fundingInfo) {
  return JSON.stringify(fundingInfo, null, 2)
}

// the human-printable version does some special things that turned out to
// be very verbose but hopefully not hard to follow: we stack up items
// that have a shared url/type and make sure they're printed at the highest
// level possible, in that process they also carry their dependencies along
// with them, moving those up in the visual tree
function printHuman (fundingInfo, opts) {
  const flatCache = fundingInfo[flatCacheSymbol]

  const { name, version } = fundingInfo
  const printableVersion = version ? `@${version}` : ''

  const items = Object.keys(flatCache).map((url) => {
    const deps = flatCache[url]

    const packages = deps.map((dep) => {
      const { name, version } = dep

      const printableVersion = version ? `@${version}` : ''
      return `${name}${printableVersion}`
    })

    return {
      label: url,
      nodes: [packages.join(', ')]
    }
  })

  return archy({ label: `${name}${printableVersion}`, nodes: items }, '', { unicode: opts.unicode })
}

function openFundingUrl (packageName, fundingSourceNumber, cb) {
  function getUrlAndOpen (packageMetadata) {
    const { funding } = packageMetadata
    const validSources = [].concat(retrieveFunding(funding)).filter(validFundingField)

    if (validSources.length === 1 || (fundingSourceNumber > 0 && fundingSourceNumber <= validSources.length)) {
      const { type, url } = validSources[fundingSourceNumber ? fundingSourceNumber - 1 : 0]
      const typePrefix = type ? `${type} funding` : 'Funding'
      const msg = `${typePrefix} available at the following URL`
      openUrl(url, msg, cb)
    } else if (!(fundingSourceNumber >= 1)) {
      validSources.forEach(({ type, url }, i) => {
        const typePrefix = type ? `${type} funding` : 'Funding'
        const msg = `${typePrefix} available at the following URL`
        console.log(`${i + 1}: ${msg}: ${url}`)
      })
      console.log('Run `npm fund [<@scope>/]<pkg> --which=1`, for example, to open the first funding URL listed in that package')
      cb()
    } else {
      const noFundingError = new Error(`No valid funding method available for: ${packageName}`)
      noFundingError.code = 'ENOFUND'

      throw noFundingError
    }
  }

  fetchPackageMetadata(
    packageName,
    '.',
    { fullMetadata: true },
    function (err, packageMetadata) {
      if (err) return cb(err)
      getUrlAndOpen(packageMetadata)
    }
  )
}

function fundCmd (args, cb) {
  const opts = FundConfig(npmConfig())
  const dir = path.resolve(npm.dir, '..')
  const packageName = args[0]
  const numberArg = opts.which

  const fundingSourceNumber = numberArg && parseInt(numberArg, 10)

  if (numberArg !== undefined && (String(fundingSourceNumber) !== numberArg || fundingSourceNumber < 1)) {
    const err = new Error('`npm fund [<@scope>/]<pkg> [--which=fundingSourceNumber]` must be given a positive integer')
    err.code = 'EFUNDNUMBER'
    throw err
  }

  if (opts.global) {
    const err = new Error('`npm fund` does not support global packages')
    err.code = 'EFUNDGLOBAL'
    throw err
  }

  if (packageName) {
    openFundingUrl(packageName, fundingSourceNumber, cb)
    return
  }

  readPackageTree(dir, function (err, tree) {
    if (err) {
      process.exitCode = 1
      return cb(err)
    }

    readShrinkwrap.andInflate(tree, function () {
      const fundingInfo = getFundingInfo(
        mutateIntoLogicalTree.asReadInstalled(
          computeMetadata(tree)
        )
      )

      const print = opts.json
        ? printJSON
        : printHuman

      output(
        print(
          fundingInfo,
          opts
        )
      )
      cb(err, tree)
    })
  })
}
