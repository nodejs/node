var Conf = require('../config/core.js').Conf
var CachingRegClient = require('../cache/caching-client.js')
var log = require('npmlog')

module.exports = getPublishConfig

function getPublishConfig (publishConfig, defaultConfig, defaultClient) {
  var config = defaultConfig
  var client = defaultClient
  log.verbose('getPublishConfig', publishConfig)
  if (publishConfig) {
    config = new Conf(defaultConfig)
    config.save = defaultConfig.save.bind(defaultConfig)

    // don't modify the actual publishConfig object, in case we have
    // to set a login token or some other data.
    config.unshift(Object.keys(publishConfig).reduce(function (s, k) {
      s[k] = publishConfig[k]
      return s
    }, {}))
    client = new CachingRegClient(config)
  }

  return { config: config, client: client }
}
