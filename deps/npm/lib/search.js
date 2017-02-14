'use strict'

module.exports = exports = search

var npm = require('./npm.js')
var allPackageMetadata = require('./search/all-package-metadata.js')
var packageFilter = require('./search/package-filter.js')
var formatPackageStream = require('./search/format-package-stream.js')
var usage = require('./utils/usage')
var output = require('./utils/output.js')
var log = require('npmlog')
var ms = require('mississippi')

search.usage = usage(
  'search',
  'npm search [--long] [search terms ...]'
)

search.completion = function (opts, cb) {
  cb(null, [])
}

function search (args, cb) {
  var staleness = npm.config.get('searchstaleness')

  var include = prepareIncludes(args, npm.config.get('searchopts'))
  if (include.length === 0) {
    return cb(new Error('search must be called with arguments'))
  }

  var exclude = prepareExcludes(npm.config.get('searchexclude'))

  // Used later to figure out whether we had any packages go out
  var anyOutput = false

  // Get a stream with *all* the packages. This takes care of dealing
  // with the local cache as well, but that's an internal detail.
  var allEntriesStream = allPackageMetadata(staleness)

  // Grab a stream that filters those packages according to given params.
  var searchSection = (npm.config.get('unicode') ? '🤔 ' : '') + 'search'
  var filterStream = streamFilter(function (pkg) {
    log.gauge.pulse('search')
    log.gauge.show({section: searchSection, logline: 'scanning ' + pkg.name})
    // Simply 'true' if the package matches search parameters.
    var match = packageFilter(pkg, include, exclude, {
      description: npm.config.get('description')
    })
    if (match) { anyOutput = true }
    return match
  })

  // Grab a configured output stream that will spit out packages in the
  // desired format.
  var outputStream = formatPackageStream({
    args: args, // --searchinclude options are not highlighted
    long: npm.config.get('long'),
    description: npm.config.get('description'),
    json: npm.config.get('json'),
    parseable: npm.config.get('parseable'),
    color: npm.color
  })
  outputStream.on('data', function (chunk) {
    output(chunk.toString('utf8'))
  })

  log.silly('search', 'searching packages')
  ms.pipe(allEntriesStream, filterStream, outputStream, function (er) {
    if (er) return cb(er)
    if (!anyOutput && !npm.config.get('json') && !npm.config.get('parseable')) {
      output('No matches found for ' + (args.map(JSON.stringify).join(' ')))
    }
    log.silly('search', 'index search completed')
    log.clearProgress()
    cb(null, {})
  })
}

function prepareIncludes (args, searchopts) {
  if (typeof searchopts !== 'string') searchopts = ''
  return searchopts.split(/\s+/).concat(args).map(function (s) {
    return s.toLowerCase()
  }).filter(function (s) { return s })
}

function prepareExcludes (searchexclude) {
  var exclude
  if (typeof searchexclude === 'string') {
    exclude = searchexclude.split(/\s+/)
  } else {
    exclude = []
  }
  return exclude.map(function (s) {
    return s.toLowerCase()
  })
}

function streamFilter (filter) {
  return ms.through.obj(function (chunk, enc, cb) {
    if (filter(chunk)) {
      this.push(chunk)
    }
    cb()
  })
}
