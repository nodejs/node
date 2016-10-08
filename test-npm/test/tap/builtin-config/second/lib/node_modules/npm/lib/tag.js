// turns out tagging isn't very complicated
// all the smarts are in the couch.
module.exports = tag
tag.usage = '[DEPRECATED] npm tag <name>@<version> [<tag>]' +
            '\nSee `dist-tag`'

tag.completion = require('./unpublish.js').completion

var npm = require('./npm.js')
var mapToRegistry = require('./utils/map-to-registry.js')
var npa = require('npm-package-arg')
var semver = require('semver')
var log = require('npmlog')

function tag (args, cb) {
  var thing = npa(args.shift() || '')
  var project = thing.name
  var version = thing.rawSpec
  var t = args.shift() || npm.config.get('tag')

  t = t.trim()

  if (!project || !version || !t) return cb('Usage:\n' + tag.usage)

  if (semver.validRange(t)) {
    var er = new Error('Tag name must not be a valid SemVer range: ' + t)
    return cb(er)
  }

  log.warn('tag', 'This command is deprecated. Use `npm dist-tag` instead.')

  mapToRegistry(project, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    var params = {
      version: version,
      tag: t,
      auth: auth
    }
    npm.registry.tag(uri, params, cb)
  })
}
