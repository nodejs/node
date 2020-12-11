const Minipass = require('minipass')
const Pipeline = require('minipass-pipeline')
const libSearch = require('libnpmsearch')
const log = require('npmlog')

const formatPackageStream = require('./search/format-package-stream.js')
const packageFilter = require('./search/package-filter.js')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const completion = require('./utils/completion/none.js')

const usage = usageUtil(
  'search',
  'npm search [--long] [search terms ...]'
)

const cmd = (args, cb) => search(args).then(() => cb()).catch(cb)

const search = async (args) => {
  const opts = {
    ...npm.flatOptions,
    ...npm.flatOptions.search,
    include: prepareIncludes(args, npm.flatOptions.search.opts),
    exclude: prepareExcludes(npm.flatOptions.search.exclude),
  }

  if (opts.include.length === 0)
    throw new Error('search must be called with arguments')

  // Used later to figure out whether we had any packages go out
  let anyOutput = false

  class FilterStream extends Minipass {
    write (pkg) {
      if (packageFilter(pkg, opts.include, opts.exclude))
        super.write(pkg)
    }
  }

  const filterStream = new FilterStream()

  // Grab a configured output stream that will spit out packages in the
  // desired format.
  const outputStream = formatPackageStream({
    args, // --searchinclude options are not highlighted
    ...opts,
  })

  log.silly('search', 'searching packages')
  const p = new Pipeline(
    libSearch.stream(opts.include, opts),
    filterStream,
    outputStream
  )

  p.on('data', chunk => {
    if (!anyOutput)
      anyOutput = true
    output(chunk.toString('utf8'))
  })

  await p.promise()
  if (!anyOutput && !opts.json && !opts.parseable)
    output('No matches found for ' + (args.map(JSON.stringify).join(' ')))

  log.silly('search', 'search completed')
  log.clearProgress()
}

function prepareIncludes (args, searchopts) {
  return args
    .map(s => s.toLowerCase())
    .filter(s => s)
}

function prepareExcludes (searchexclude) {
  var exclude
  if (typeof searchexclude === 'string')
    exclude = searchexclude.split(/\s+/)
  else
    exclude = []

  return exclude
    .map(s => s.toLowerCase())
    .filter(s => s)
}

module.exports = Object.assign(cmd, { completion, usage })
