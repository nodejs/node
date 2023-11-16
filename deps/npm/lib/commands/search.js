const { Minipass } = require('minipass')
const Pipeline = require('minipass-pipeline')
const libSearch = require('libnpmsearch')
const log = require('../utils/log-shim.js')

const formatSearchStream = require('../utils/format-search-stream.js')

function filter (data, include, exclude) {
  const words = [data.name]
    .concat(data.maintainers.map(m => `=${m.username}`))
    .concat(data.keywords || [])
    .map(f => f && f.trim && f.trim())
    .filter(f => f)
    .join(' ')
    .toLowerCase()

  if (exclude.find(e => match(words, e))) {
    return false
  }

  return true
}

function match (words, pattern) {
  if (pattern.startsWith('/')) {
    if (pattern.endsWith('/')) {
      pattern = pattern.slice(0, -1)
    }
    pattern = new RegExp(pattern.slice(1))
    return words.match(pattern)
  }
  return words.indexOf(pattern) !== -1
}

const BaseCommand = require('../base-command.js')
class Search extends BaseCommand {
  static description = 'Search for packages'
  static name = 'search'
  static params = [
    'long',
    'json',
    'color',
    'parseable',
    'description',
    'searchopts',
    'searchexclude',
    'registry',
    'prefer-online',
    'prefer-offline',
    'offline',
  ]

  static usage = ['[search terms ...]']

  async exec (args) {
    const opts = {
      ...this.npm.flatOptions,
      ...this.npm.flatOptions.search,
      include: args.map(s => s.toLowerCase()).filter(s => s),
      exclude: this.npm.flatOptions.search.exclude.split(/\s+/),
    }

    if (opts.include.length === 0) {
      throw new Error('search must be called with arguments')
    }

    // Used later to figure out whether we had any packages go out
    let anyOutput = false

    class FilterStream extends Minipass {
      constructor () {
        super({ objectMode: true })
      }

      write (pkg) {
        if (filter(pkg, opts.include, opts.exclude)) {
          super.write(pkg)
        }
      }
    }

    const filterStream = new FilterStream()

    const { default: stripAnsi } = await import('strip-ansi')
    // Grab a configured output stream that will spit out packages in the desired format.
    const outputStream = await formatSearchStream({
      args, // --searchinclude options are not highlighted
      ...opts,
    }, stripAnsi)

    log.silly('search', 'searching packages')
    const p = new Pipeline(
      libSearch.stream(opts.include, opts),
      filterStream,
      outputStream
    )

    p.on('data', chunk => {
      if (!anyOutput) {
        anyOutput = true
      }
      this.npm.output(chunk.toString('utf8'))
    })

    await p.promise()
    if (!anyOutput && !this.npm.config.get('json') && !this.npm.config.get('parseable')) {
      this.npm.output('No matches found for ' + (args.map(JSON.stringify).join(' ')))
    }

    log.silly('search', 'search completed')
    log.clearProgress()
  }
}
module.exports = Search
