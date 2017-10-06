module.exports = docs

var npm = require('./npm.js')
var opener = require('opener')
var log = require('npmlog')
var fetchPackageMetadata = require('./fetch-package-metadata.js')
var usage = require('./utils/usage')

docs.usage = usage(
  'docs',
  'npm docs <pkgname>' +
  '\nnpm docs .'
)
docs.completion = function (opts, cb) {
  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

function docs (args, cb) {
  if (!args || !args.length) args = ['.']
  var pending = args.length
  log.silly('docs', args)
  args.forEach(function (proj) {
    getDoc(proj, function (err) {
      if (err) {
        return cb(err)
      }
      --pending || cb()
    })
  })
}

function getDoc (project, cb) {
  log.silly('getDoc', project)
  fetchPackageMetadata(project, '.', {fullMetadata: true}, function (er, d) {
    if (er) return cb(er)
    var url = d.homepage
    if (!url) url = 'https://www.npmjs.org/package/' + d.name
    return opener(url, {command: npm.config.get('browser')}, cb)
  })
}
