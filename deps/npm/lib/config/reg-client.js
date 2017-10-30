'use strict'

module.exports = regClientConfig
function regClientConfig (npm, log, config) {
  return {
    proxy: {
      http: config.get('proxy'),
      https: config.get('https-proxy'),
      localAddress: config.get('local-address')
    },
    ssl: {
      certificate: config.get('cert'),
      key: config.get('key'),
      ca: config.get('ca'),
      strict: config.get('strict-ssl')
    },
    retry: {
      retries: config.get('fetch-retries'),
      factor: config.get('fetch-retry-factor'),
      minTimeout: config.get('fetch-retry-mintimeout'),
      maxTimeout: config.get('fetch-retry-maxtimeout')
    },
    userAgent: config.get('user-agent'),
    log: log,
    defaultTag: config.get('tag'),
    maxSockets: config.get('maxsockets'),
    scope: npm.projectScope
  }
}
