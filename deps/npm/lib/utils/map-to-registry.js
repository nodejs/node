var url = require('url')

var log = require('npmlog')
var npa = require('npm-package-arg')
var config

module.exports = mapToRegistry

function mapToRegistry (name, config, cb) {
  log.silly('mapToRegistry', 'name', name)
  var registry

  // the name itself takes precedence
  var data = npa(name)
  if (data.scope) {
    // the name is definitely scoped, so escape now
    name = name.replace('/', '%2f')

    log.silly('mapToRegistry', 'scope (from package name)', data.scope)

    registry = config.get(data.scope + ':registry')
    if (!registry) {
      log.verbose('mapToRegistry', 'no registry URL found in name for scope', data.scope)
    }
  }

  // ...then --scope=@scope or --scope=scope
  var scope = config.get('scope')
  if (!registry && scope) {
    // I'm an enabler, sorry
    if (scope.charAt(0) !== '@') scope = '@' + scope

    log.silly('mapToRegistry', 'scope (from config)', scope)

    registry = config.get(scope + ':registry')
    if (!registry) {
      log.verbose('mapToRegistry', 'no registry URL found in config for scope', scope)
    }
  }

  // ...and finally use the default registry
  if (!registry) {
    log.silly('mapToRegistry', 'using default registry')
    registry = config.get('registry')
  }

  log.silly('mapToRegistry', 'registry', registry)

  var auth = config.getCredentialsByURI(registry)

  // normalize registry URL so resolution doesn't drop a piece of registry URL
  var normalized = registry.slice(-1) !== '/' ? registry + '/' : registry
  var uri
  log.silly('mapToRegistry', 'data', data)
  if (data.type === 'remote') {
    uri = data.fetchSpec
  } else {
    uri = url.resolve(normalized, name)
  }

  log.silly('mapToRegistry', 'uri', uri)

  cb(null, uri, scopeAuth(uri, registry, auth), normalized)
}

function scopeAuth (uri, registry, auth) {
  var cleaned = {
    scope: auth.scope,
    email: auth.email,
    alwaysAuth: auth.alwaysAuth,
    token: undefined,
    username: undefined,
    password: undefined,
    auth: undefined
  }

  var requestHost
  var registryHost

  if (auth.token || auth.auth || (auth.username && auth.password)) {
    requestHost = url.parse(uri).hostname
    registryHost = url.parse(registry).hostname

    if (requestHost === registryHost) {
      cleaned.token = auth.token
      cleaned.auth = auth.auth
      cleaned.username = auth.username
      cleaned.password = auth.password
    } else if (auth.alwaysAuth) {
      log.verbose('scopeAuth', 'alwaysAuth set for', registry)
      cleaned.token = auth.token
      cleaned.auth = auth.auth
      cleaned.username = auth.username
      cleaned.password = auth.password
    } else {
      log.silly('scopeAuth', uri, "doesn't share host with registry", registry)
    }
    if (!config) config = require('../npm').config
    if (config.get('otp')) cleaned.otp = config.get('otp')
  }

  return cleaned
}
