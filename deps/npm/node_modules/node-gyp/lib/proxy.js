'use strict'
// Taken from https://github.com/request/request/blob/212570b/lib/getProxyFromURI.js

const url = require('url')

function formatHostname (hostname) {
  // canonicalize the hostname, so that 'oogle.com' won't match 'google.com'
  return hostname.replace(/^\.*/, '.').toLowerCase()
}

function parseNoProxyZone (zone) {
  zone = zone.trim().toLowerCase()

  var zoneParts = zone.split(':', 2)
  var zoneHost = formatHostname(zoneParts[0])
  var zonePort = zoneParts[1]
  var hasPort = zone.indexOf(':') > -1

  return { hostname: zoneHost, port: zonePort, hasPort: hasPort }
}

function uriInNoProxy (uri, noProxy) {
  var port = uri.port || (uri.protocol === 'https:' ? '443' : '80')
  var hostname = formatHostname(uri.hostname)
  var noProxyList = noProxy.split(',')

  // iterate through the noProxyList until it finds a match.
  return noProxyList.map(parseNoProxyZone).some(function (noProxyZone) {
    var isMatchedAt = hostname.indexOf(noProxyZone.hostname)
    var hostnameMatched = (
      isMatchedAt > -1 &&
        (isMatchedAt === hostname.length - noProxyZone.hostname.length)
    )

    if (noProxyZone.hasPort) {
      return (port === noProxyZone.port) && hostnameMatched
    }

    return hostnameMatched
  })
}

function getProxyFromURI (gyp, env, uri) {
  // If a string URI/URL was given, parse it into a URL object
  if (typeof uri === 'string') {
    // eslint-disable-next-line
    uri = url.parse(uri)
  }

  // Decide the proper request proxy to use based on the request URI object and the
  // environmental variables (NO_PROXY, HTTP_PROXY, etc.)
  // respect NO_PROXY environment variables (see: https://lynx.invisible-island.net/lynx2.8.7/breakout/lynx_help/keystrokes/environments.html)

  var noProxy = gyp.opts.noproxy || env.NO_PROXY || env.no_proxy || env.npm_config_noproxy || ''

  // if the noProxy is a wildcard then return null

  if (noProxy === '*') {
    return null
  }

  // if the noProxy is not empty and the uri is found return null

  if (noProxy !== '' && uriInNoProxy(uri, noProxy)) {
    return null
  }

  // Check for HTTP or HTTPS Proxy in environment Else default to null

  if (uri.protocol === 'http:') {
    return gyp.opts.proxy ||
      env.HTTP_PROXY ||
      env.http_proxy ||
      env.npm_config_proxy || null
  }

  if (uri.protocol === 'https:') {
    return gyp.opts.proxy ||
      env.HTTPS_PROXY ||
      env.https_proxy ||
      env.HTTP_PROXY ||
      env.http_proxy ||
      env.npm_config_proxy || null
  }

  // if none of that works, return null
  // (What uri protocol are you using then?)

  return null
}

module.exports = getProxyFromURI
