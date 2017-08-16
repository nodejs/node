'use strict'

const clientConfig = require('../config/reg-client.js')
const Conf = require('../config/core.js').Conf
const log = require('npmlog')
const npm = require('../npm.js')
const RegClient = require('npm-registry-client')

module.exports = getPublishConfig

function getPublishConfig (publishConfig, defaultConfig, defaultClient) {
  let config = defaultConfig
  let client = defaultClient
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
    client = new RegClient(clientConfig(npm, log, config))
  }

  return { config: config, client: client }
}
