module.exports = docs

docs.usage  = "npm docs <pkgname>"
docs.usage += "\n"
docs.usage += "npm docs ."

docs.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb()
  registry.get("/-/short", 60000, function (er, list) {
    return cb(null, list || [])
  })
}

var npm = require("./npm.js")
  , registry = npm.registry
  , opener = require("opener")
  , path = require('path')
  , log = require('npmlog')

function url (json) {
  return json.homepage ? json.homepage : "https://npmjs.org/package/" + json.name
}

function docs (args, cb) {
  var project = args[0] || '.'
    , package = path.resolve(process.cwd(), "package.json")

  if (project === '.') {
    try {
      var json = require(package)
      if (!json.name) throw new Error('package.json does not have a valid "name" property')
      project = json.name
    } catch (e) {
      log.error(e.message)
      return cb(docs.usage)
    }

    return opener(url(json), { command: npm.config.get("browser") }, cb)
  }

  registry.get(project + "/latest", 3600, function (er, json) {
    var github = "https://github.com/" + project + "#readme"

    if (er) {
      if (project.split("/").length !== 2) return cb(er)
      return opener(github, { command: npm.config.get("browser") }, cb)
    }

    return opener(url(json), { command: npm.config.get("browser") }, cb)
  })
}
