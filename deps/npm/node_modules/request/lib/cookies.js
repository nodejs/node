'use strict'

var tough = require('tough-cookie')

var Cookie = tough.Cookie
  , CookieJar = tough.CookieJar


exports.parse = function(str) {
  if (str && str.uri) {
    str = str.uri
  }
  if (typeof str !== 'string') {
    throw new Error('The cookie function only accepts STRING as param')
  }
  if (!Cookie) {
    return null
  }
  return Cookie.parse(str)
}

// Adapt the sometimes-Async api of tough.CookieJar to our requirements
function RequestJar() {
  var self = this
  self._jar = new CookieJar()
}
RequestJar.prototype.setCookie = function(cookieOrStr, uri, options) {
  var self = this
  return self._jar.setCookieSync(cookieOrStr, uri, options || {})
}
RequestJar.prototype.getCookieString = function(uri) {
  var self = this
  return self._jar.getCookieStringSync(uri)
}
RequestJar.prototype.getCookies = function(uri) {
  var self = this
  return self._jar.getCookiesSync(uri)
}

exports.jar = function() {
  if (!CookieJar) {
    // tough-cookie not loaded, return a stub object:
    return {
      setCookie: function(){},
      getCookieString: function(){},
      getCookies: function(){}
    }
  }
  return new RequestJar()
}
