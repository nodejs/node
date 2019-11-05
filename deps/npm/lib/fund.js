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
const { getFundingInfo, validFundingUrl } = require('./utils/funding.js')

const FundConfig = figgyPudding({
  browser: {}, // used by ./utils/open-url
  global: {},
  json: {},
  unicode: {}
})

module.exports = fundCmd

const usage = require('./utils/usage')
fundCmd.usage = usage(
  'fund',
  'npm fund [--json]',
  'npm fund [--browser] [[<@scope>/]<pkg>'
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
  // mapping logic that keeps track of seen items in order to be able
  // to push all other items from the same type/url in the same place
  const seen = new Map()

  function seenKey ({ type, url } = {}) {
    return url ? String(type) + String(url) : null
  }

  function setStackedItem (funding, result) {
    const key = seenKey(funding)
    if (key && !seen.has(key)) seen.set(key, result)
  }

  function retrieveStackedItem (funding) {
    const key = seenKey(funding)
    if (key && seen.has(key)) return seen.get(key)
  }

  // ---

  const getFundingItems = (fundingItems) =>
    Object.keys(fundingItems || {}).map((fundingItemName) => {
      // first-level loop, prepare the pretty-printed formatted data
      const fundingItem = fundingItems[fundingItemName]
      const { version, funding } = fundingItem
      const { type, url } = funding || {}

      const printableVersion = version ? `@${version}` : ''
      const printableType = type && { label: `type: ${funding.type}` }
      const printableUrl = url && { label: `url: ${funding.url}` }
      const result = {
        fundingItem,
        label: fundingItemName + printableVersion,
        nodes: []
      }

      if (printableType) {
        result.nodes.push(printableType)
      }

      if (printableUrl) {
        result.nodes.push(printableUrl)
      }

      setStackedItem(funding, result)

      return result
    }).reduce((res, result) => {
      // recurse and exclude nodes that are going to be stacked together
      const { fundingItem } = result
      const { dependencies, funding } = fundingItem
      const items = getFundingItems(dependencies)
      const stackedResult = retrieveStackedItem(funding)
      items.forEach(i => result.nodes.push(i))

      if (stackedResult && stackedResult !== result) {
        stackedResult.label += `, ${result.label}`
        items.forEach(i => stackedResult.nodes.push(i))
        return res
      }

      res.push(result)

      return res
    }, [])

  const [ result ] = getFundingItems({
    [fundingInfo.name]: {
      dependencies: fundingInfo.dependencies,
      funding: fundingInfo.funding,
      version: fundingInfo.version
    }
  })

  return archy(result, '', { unicode: opts.unicode })
}

function openFundingUrl (packageName, cb) {
  function getUrlAndOpen (packageMetadata) {
    const { funding } = packageMetadata
    const { type, url } = funding || {}
    const noFundingError =
      new Error(`No funding method available for: ${packageName}`)
    noFundingError.code = 'ENOFUND'
    const typePrefix = type ? `${type} funding` : 'Funding'
    const msg = `${typePrefix} available at the following URL`

    if (validFundingUrl(funding)) {
      openUrl(url, msg, cb)
    } else {
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

  if (opts.global) {
    const err = new Error('`npm fund` does not support globals')
    err.code = 'EFUNDGLOBAL'
    throw err
  }

  if (packageName) {
    openFundingUrl(packageName, cb)
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
