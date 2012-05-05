// get an http/https agent
// This is necessary for the custom CA certs in http2,
// especially while juggling multiple different registries.
//
// When using http2, the agent key is just the CA setting,
// since it can manage socket pooling across different host:port
// options.  When using the older implementation, the
// key is ca:host:port combination.

module.exports = getAgent

var npm = require("../npm.js")
  , url = require("url")
  , agents = {}
  , isHttp2 = !!require("http").globalAgent
  , registry = url.parse(npm.config.get("registry") || "")
  , regCA = npm.config.get("ca")

function getAgent (remote) {
  // If not doing https, then there's no CA cert to manage.
  // on http2, this will use the default global agent.
  // on http1, this is undefined, so it'll spawn based on
  // host:port if necessary.
  if (remote.protocol !== "https:") {
    return require("http").globalAgent
  }

  if (typeof remote === "string") {
    remote = url.parse(remote)
  }

  var ca
  // if this is the registry, then use the configuration ca.
  // otherwise, just use the built-in CAs that node has.
  // todo: multi-registry support.
  if (remote.hostname === registry.hostname
      && remote.port === registry.port) {
    ca = regCA
  }

  // no CA, just use the default agent.
  if (!ca) {
    return require("https").globalAgent
  }

  var hostname = remote.hostname
    , port = remote.port
    , key = agentKey(hostname, port, ca)

  return agents[key] = agents[key] || getAgent_(hostname, port, ca)
}

function getAgent_ (hostname, port, ca) {
  var Agent = require("https").Agent
  return new Agent({ host: hostname
                   , port: port
                   , ca: ca })
}

function agentKey (hostname, port, ca) {
  return JSON.stringify(isHttp2 ? ca : [hostname, port, ca])
}
