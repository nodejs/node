module.exports = bugs

bugs.usage = 'npm bugs [<pkgname>]'

var npm = require('./npm.js')
var log = require('npmlog')
var opener = require('opener')
var fetchPackageMetadata = require('./fetch-package-metadata.js')

bugs.completion = function (opts, cb) {
  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

function bugs (args, cb) {
  var n = args.length ? args[0] : '.'
  fetchPackageMetadata(n, '.', function (er, d) {
    if (er) return cb(er)

    var url = d.bugs && ((typeof d.bugs === 'string') ? d.bugs : d.bugs.url)
    if (!url) {
      url = 'https://www.npmjs.org/package/' + d.name
    }
    log.silly('bugs', 'url', url)
    opener(url, { command: npm.config.get('browser') }, cb)
  })
}
