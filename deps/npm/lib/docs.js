module.exports = docs

docs.usage  = "npm docs <pkgname>"
docs.usage += "\n"
docs.usage += "npm docs ."

docs.completion = function (opts, cb) {
  var uri = url_.resolve(npm.config.get("registry"), "/-/short")
  registry.get(uri, { timeout : 60000 }, function (er, list) {
    return cb(null, list || [])
  })
}

var url_ = require("url")
  , npm = require("./npm.js")
  , registry = npm.registry
  , opener = require("opener")
  , path = require("path")
  , log = require("npmlog")

function url (json) {
  return json.homepage ? json.homepage : "https://npmjs.org/package/" + json.name
}

function docs (args, cb) {
  args = args || []
  var pending = args.length
  if (!pending) return getDoc('.', cb)
  args.forEach(function(proj) {
    getDoc(proj, function(err) {
      if (err) {
        return cb(err)
      }
      --pending || cb()
    })
  })
}

function getDoc (project, cb) {
  project = project || '.'
  var package = path.resolve(process.cwd(), "package.json")

  if (project === '.' || project === './') {
    var json
    try {
      json = require(package)
      if (!json.name) throw new Error('package.json does not have a valid "name" property')
      project = json.name
    } catch (e) {
      log.error(e.message)
      return cb(docs.usage)
    }

    return opener(url(json), { command: npm.config.get("browser") }, cb)
  }

  var uri = url_.resolve(npm.config.get("registry"), project + "/latest")
  registry.get(uri, { timeout : 3600 }, function (er, json) {
    var github = "https://github.com/" + project + "#readme"

    if (er) {
      if (project.split("/").length !== 2) return cb(er)
      return opener(github, { command: npm.config.get("browser") }, cb)
    }

    return opener(url(json), { command: npm.config.get("browser") }, cb)
  })
}
