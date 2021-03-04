const Minipass = require('minipass')
const Pipeline = require('minipass-pipeline')
const libSearch = require('libnpmsearch')
const log = require('npmlog')

const formatPackageStream = require('./search/format-package-stream.js')
const packageFilter = require('./search/package-filter.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')

function prepareIncludes (args) {
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

class Search {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'search',
      'npm search [-l|--long] [--json] [--parseable] [--no-description] [search terms ...]'
    )
  }

  exec (args, cb) {
    this.search(args).then(() => cb()).catch(cb)
  }

  async search (args) {
    const opts = {
      ...this.npm.flatOptions,
      ...this.npm.flatOptions.search,
      include: prepareIncludes(args),
      exclude: prepareExcludes(this.npm.flatOptions.search.exclude),
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
}
module.exports = Search
