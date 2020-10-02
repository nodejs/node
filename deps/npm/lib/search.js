'use strict'

module.exports = exports = search

const Pipeline = require('minipass-pipeline')

const npm = require('./npm.js')
const formatPackageStream = require('./search/format-package-stream.js')

const libSearch = require('libnpmsearch')
const log = require('npmlog')
const output = require('./utils/output.js')
const usage = require('./utils/usage')

search.usage = usage(
  'search',
  'npm search [--long] [search terms ...]'
)

search.completion = function (opts, cb) {
  cb(null, [])
}

function search (args, cb) {
  const opts = {
    ...npm.flatOptions,
    ...npm.flatOptions.search,
    include: prepareIncludes(args, npm.flatOptions.search.opts),
    exclude: prepareExcludes(npm.flatOptions.search.exclude)
  }

  if (opts.include.length === 0) {
    return cb(new Error('search must be called with arguments'))
  }

  // Used later to figure out whether we had any packages go out
  let anyOutput = false

  // Grab a configured output stream that will spit out packages in the
  // desired format.
  //
  // This is a text minipass stream
  var outputStream = formatPackageStream({
    args, // --searchinclude options are not highlighted
    ...opts
  })

  log.silly('search', 'searching packages')
  const p = new Pipeline(libSearch.stream(opts.include, opts), outputStream)

  p.on('data', chunk => {
    if (!anyOutput) { anyOutput = true }
    output(chunk.toString('utf8'))
  })

  p.promise().then(() => {
    if (!anyOutput && !opts.json && !opts.parseable) {
      output('No matches found for ' + (args.map(JSON.stringify).join(' ')))
    }
    log.silly('search', 'search completed')
    log.clearProgress()
    cb(null, {})
  }, err => cb(err))
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
