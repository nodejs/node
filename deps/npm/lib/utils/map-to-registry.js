var url = require("url")

var log = require("npmlog")
  , npa = require("npm-package-arg")

module.exports = mapToRegistry

function mapToRegistry(name, config, cb) {
  var uri
  var scopedRegistry

  // the name itself takes precedence
  var data = npa(name)
  if (data.scope) {
    // the name is definitely scoped, so escape now
    name = name.replace("/", "%2f")

    log.silly("mapToRegistry", "scope", data.scope)

    scopedRegistry = config.get(data.scope + ":registry")
    if (scopedRegistry) {
      log.silly("mapToRegistry", "scopedRegistry (scoped package)", scopedRegistry)
      uri = url.resolve(scopedRegistry, name)
    }
    else {
      log.verbose("mapToRegistry", "no registry URL found for scope", data.scope)
    }
  }

  // ...then --scope=@scope or --scope=scope
  var scope = config.get("scope")
  if (!uri && scope) {
    // I'm an enabler, sorry
    if (scope.charAt(0) !== "@") scope = "@" + scope

    scopedRegistry = config.get(scope + ":registry")
    if (scopedRegistry) {
      log.silly("mapToRegistry", "scopedRegistry (scope in config)", scopedRegistry)
      uri = url.resolve(scopedRegistry, name)
    }
    else {
      log.verbose("mapToRegistry", "no registry URL found for scope", scope)
    }
  }

  // ...and finally use the default registry
  if (!uri) {
    uri = url.resolve(config.get("registry"), name)
  }

  log.verbose("mapToRegistry", "name", name)
  log.verbose("mapToRegistry", "uri", uri)
  cb(null, uri)
}
