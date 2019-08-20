var ms = require('mississippi')
var allPackageMetadata = require('./all-package-metadata')
var packageFilter = require('./package-filter.js')

module.exports = allPackageSearch
function allPackageSearch (opts) {
  var searchSection = (opts.unicode ? '🤔 ' : '') + 'search'

  // Get a stream with *all* the packages. This takes care of dealing
  // with the local cache as well, but that's an internal detail.
  var allEntriesStream = allPackageMetadata(opts)

  // Grab a stream that filters those packages according to given params.
  var filterStream = streamFilter(function (pkg) {
    opts.log.gauge.pulse('search')
    opts.log.gauge.show({section: searchSection, logline: 'scanning ' + pkg.name})
    // Simply 'true' if the package matches search parameters.
    var match = packageFilter(pkg, opts.include, opts.exclude, {
      description: opts.description
    })
    return match
  })
  return ms.pipeline.obj(allEntriesStream, filterStream)
}

function streamFilter (filter) {
  return ms.through.obj(function (data, enc, cb) {
    if (filter(data)) {
      this.push(standardizePkg(data))
    }
    cb()
  })
}

function standardizePkg (data) {
  return {
    name: data.name,
    description: data.description,
    maintainers: (data.maintainers || []).map(function (m) {
      return { username: m.name, email: m.email }
    }),
    keywords: data.keywords || [],
    version: Object.keys(data.versions || {})[0] || [],
    date: (
      data.time &&
      data.time.modified &&
      new Date(data.time.modified)
    ) || null
  }
}
