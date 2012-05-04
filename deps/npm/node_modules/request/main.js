// Copyright 2010-2011 Mikeal Rogers
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

var http = require('http')
  , https = false
  , tls = false
  , url = require('url')
  , util = require('util')
  , stream = require('stream')
  , qs = require('querystring')
  , mimetypes = require('./mimetypes')
  , oauth = require('./oauth')
  , uuid = require('./uuid')
  , ForeverAgent = require('./forever')
  , Cookie = require('./vendor/cookie')
  , CookieJar = require('./vendor/cookie/jar')
  , cookieJar = new CookieJar
  ;
  
if (process.logging) {
  var log = process.logging('request')
}

try {
  https = require('https')
} catch (e) {}

try {
  tls = require('tls')
} catch (e) {}

function toBase64 (str) {
  return (new Buffer(str || "", "ascii")).toString("base64")
}

// Hacky fix for pre-0.4.4 https
if (https && !https.Agent) {
  https.Agent = function (options) {
    http.Agent.call(this, options)
  }
  util.inherits(https.Agent, http.Agent)
  https.Agent.prototype._getConnection = function(host, port, cb) {
    var s = tls.connect(port, host, this.options, function() {
      // do other checks here?
      if (cb) cb()
    })
    return s
  }
}

function isReadStream (rs) {
  if (rs.readable && rs.path && rs.mode) {
    return true
  }
}

function copy (obj) {
  var o = {}
  for (var i in obj) o[i] = obj[i]
  return o
}

var isUrl = /^https?:/

var globalPool = {}

function Request (options) {
  stream.Stream.call(this)
  this.readable = true
  this.writable = true

  if (typeof options === 'string') {
    options = {uri:options}
  }

  for (var i in options) {
    this[i] = options[i]
  }
  if (!this.pool) this.pool = globalPool
  this.dests = []
  this.__isRequestRequest = true
}
util.inherits(Request, stream.Stream)
Request.prototype.getAgent = function (host, port) {
  if (!this.pool[host+':'+port]) {
    this.pool[host+':'+port] = new this.httpModule.Agent({host:host, port:port})
  }
  return this.pool[host+':'+port]
}
Request.prototype.request = function () {
  var self = this

  // Protect against double callback
  if (!self._callback && self.callback) {
    self._callback = self.callback
    self.callback = function () {
      if (self._callbackCalled) return // Print a warning maybe?
      self._callback.apply(self, arguments)
      self._callbackCalled = true
    }
  }

  if (self.url) {
    // People use this property instead all the time so why not just support it.
    self.uri = self.url
    delete self.url
  }

  if (!self.uri) {
    throw new Error("options.uri is a required argument")
  } else {
    if (typeof self.uri == "string") self.uri = url.parse(self.uri)
  }
  if (self.proxy) {
    if (typeof self.proxy == 'string') self.proxy = url.parse(self.proxy)
  }

  self._redirectsFollowed = self._redirectsFollowed || 0
  self.maxRedirects = (self.maxRedirects !== undefined) ? self.maxRedirects : 10
  self.followRedirect = (self.followRedirect !== undefined) ? self.followRedirect : true
  if (self.followRedirect)
    self.redirects = self.redirects || []

  self.headers = self.headers ? copy(self.headers) : {}

  var setHost = false
  if (!self.headers.host) {
    self.headers.host = self.uri.hostname
    if (self.uri.port) {
      if ( !(self.uri.port === 80 && self.uri.protocol === 'http:') &&
           !(self.uri.port === 443 && self.uri.protocol === 'https:') )
      self.headers.host += (':'+self.uri.port)
    }
    setHost = true
  }

  if (self.jar === false) {
    // disable cookies
    var cookies = false;
    self._disableCookies = true;
  } else if (self.jar) {
    // fetch cookie from the user defined cookie jar
    var cookies = self.jar.get({ url: self.uri.href })
  } else {
    // fetch cookie from the global cookie jar
    var cookies = cookieJar.get({ url: self.uri.href })
  }
  if (cookies) {
    var cookieString = cookies.map(function (c) {
      return c.name + "=" + c.value;
    }).join("; ");
    
    self.headers.Cookie = cookieString;
  }

  if (!self.uri.pathname) {self.uri.pathname = '/'}
  if (!self.uri.port) {
    if (self.uri.protocol == 'http:') {self.uri.port = 80}
    else if (self.uri.protocol == 'https:') {self.uri.port = 443}
  }

  if (self.proxy) {
    self.port = self.proxy.port
    self.host = self.proxy.hostname
  } else {
    self.port = self.uri.port
    self.host = self.uri.hostname
  }

  if (self.onResponse === true) {
    self.onResponse = self.callback
    delete self.callback
  }

  var clientErrorHandler = function (error) {
    if (setHost) delete self.headers.host
    if (self.req._reusedSocket && error.code === 'ECONNRESET') {
      self.agent = {addRequest: ForeverAgent.prototype.addRequestNoreuse.bind(self.agent)}
      self.start()
      self.req.end()
      return
    }
    if (self.timeout && self.timeoutTimer) clearTimeout(self.timeoutTimer)
    self.emit('error', error)
  }
  if (self.onResponse) self.on('error', function (e) {self.onResponse(e)})
  if (self.callback) self.on('error', function (e) {self.callback(e)})

  if (self.form) {
    self.headers['content-type'] = 'application/x-www-form-urlencoded; charset=utf-8'
    self.body = qs.stringify(self.form).toString('utf8')
  }

  if (self.oauth) {
    var form
    if (self.headers['content-type'] && 
        self.headers['content-type'].slice(0, 'application/x-www-form-urlencoded'.length) ===
          'application/x-www-form-urlencoded' 
       ) {
      form = qs.parse(self.body)
    } 
    if (self.uri.query) {
      form = qs.parse(self.uri.query)
    } 
    if (!form) form = {}
    var oa = {}
    for (var i in form) oa[i] = form[i]
    for (var i in self.oauth) oa['oauth_'+i] = self.oauth[i]
    if (!oa.oauth_version) oa.oauth_version = '1.0'
    if (!oa.oauth_timestamp) oa.oauth_timestamp = Math.floor( (new Date()).getTime() / 1000 ).toString()
    if (!oa.oauth_nonce) oa.oauth_nonce = uuid().replace(/-/g, '')
    
    oa.oauth_signature_method = 'HMAC-SHA1'
    
    var consumer_secret = oa.oauth_consumer_secret
    delete oa.oauth_consumer_secret
    var token_secret = oa.oauth_token_secret
    delete oa.oauth_token_secret
    
    var baseurl = self.uri.protocol + '//' + self.uri.host + self.uri.pathname
    var signature = oauth.hmacsign(self.method, baseurl, oa, consumer_secret, token_secret)
    
    // oa.oauth_signature = signature
    for (var i in form) {
      if ( i.slice(0, 'oauth_') in self.oauth) {
        // skip 
      } else {
        delete oa['oauth_'+i]
      }
    }
    self.headers.authorization = 
      'OAuth '+Object.keys(oa).sort().map(function (i) {return i+'="'+oauth.rfc3986(oa[i])+'"'}).join(',')
    self.headers.authorization += ',oauth_signature="'+oauth.rfc3986(signature)+'"'  
  }

  if (self.uri.auth && !self.headers.authorization) {
    self.headers.authorization = "Basic " + toBase64(self.uri.auth.split(':').map(function(item){ return qs.unescape(item)}).join(':'))
  }
  if (self.proxy && self.proxy.auth && !self.headers['proxy-authorization']) {
    self.headers['proxy-authorization'] = "Basic " + toBase64(self.proxy.auth.split(':').map(function(item){ return qs.unescape(item)}).join(':'))
  }

  if (self.uri.path) {
    self.path = self.uri.path
  } else {
    self.path = self.uri.pathname + (self.uri.search || "")
  }

  if (self.path.length === 0) self.path = '/'

  if (self.proxy) self.path = (self.uri.protocol + '//' + self.uri.host + self.path)

  if (self.json) {
    self.headers['content-type'] = 'application/json'
    if (typeof self.json === 'boolean') {
      if (typeof self.body === 'object') self.body = JSON.stringify(self.body)
    } else {
      self.body = JSON.stringify(self.json)
    }

  } else if (self.multipart) {
    self.body = []

    if (!self.headers['content-type']) {
      self.headers['content-type'] = 'multipart/related;boundary="frontier"';
    } else {
      self.headers['content-type'] = self.headers['content-type'].split(';')[0] + ';boundary="frontier"';
    }

    if (!self.multipart.forEach) throw new Error('Argument error, options.multipart.')

    self.multipart.forEach(function (part) {
      var body = part.body
      if(!body) throw Error('Body attribute missing in multipart.')
      delete part.body
      var preamble = '--frontier\r\n'
      Object.keys(part).forEach(function(key){
        preamble += key + ': ' + part[key] + '\r\n'
      })
      preamble += '\r\n'
      self.body.push(new Buffer(preamble))
      self.body.push(new Buffer(body))
      self.body.push(new Buffer('\r\n'))
    })
    self.body.push(new Buffer('--frontier--'))
  }

  if (self.body) {
    var length = 0
    if (!Buffer.isBuffer(self.body)) {
      if (Array.isArray(self.body)) {
        for (var i = 0; i < self.body.length; i++) {
          length += self.body[i].length
        }
      } else {
        self.body = new Buffer(self.body)
        length = self.body.length
      }
    } else {
      length = self.body.length
    }
    if (length) {
      self.headers['content-length'] = length
    } else {
      throw new Error('Argument error, options.body.')
    }
  }

  var protocol = self.proxy ? self.proxy.protocol : self.uri.protocol
    , defaultModules = {'http:':http, 'https:':https}
    , httpModules = self.httpModules || {}
    ;
  self.httpModule = httpModules[protocol] || defaultModules[protocol]

  if (!self.httpModule) throw new Error("Invalid protocol")

  if (self.pool === false) {
    self.agent = false
  } else {
    if (self.maxSockets) {
      // Don't use our pooling if node has the refactored client
      self.agent = self.agent || self.httpModule.globalAgent || self.getAgent(self.host, self.port)
      self.agent.maxSockets = self.maxSockets
    }
    if (self.pool.maxSockets) {
      // Don't use our pooling if node has the refactored client
      self.agent = self.agent || self.httpModule.globalAgent || self.getAgent(self.host, self.port)
      self.agent.maxSockets = self.pool.maxSockets
    }
  }

  self.start = function () {
    self._started = true
    self.method = self.method || 'GET'
    self.href = self.uri.href
    if (log) log('%method %href', self)
    self.req = self.httpModule.request(self, function (response) {
      self.response = response
      response.request = self

      if (self.httpModule === https &&
          self.strictSSL &&
          !response.client.authorized) {
        var sslErr = response.client.authorizationError
        self.emit('error', new Error('SSL Error: '+ sslErr))
        return
      }

      if (setHost) delete self.headers.host
      if (self.timeout && self.timeoutTimer) clearTimeout(self.timeoutTimer)
      
      if (response.headers['set-cookie'] && (!self._disableCookies)) {
        response.headers['set-cookie'].forEach(function(cookie) {
          if (self.jar) self.jar.add(new Cookie(cookie))
          else cookieJar.add(new Cookie(cookie))
        })
      }

      if (response.statusCode >= 300 &&
          response.statusCode < 400  &&
          self.followRedirect     &&
          self.method !== 'PUT' &&
          self.method !== 'POST' &&
          response.headers.location) {
        if (self._redirectsFollowed >= self.maxRedirects) {
          self.emit('error', new Error("Exceeded maxRedirects. Probably stuck in a redirect loop."))
          return
        }
        self._redirectsFollowed += 1

        if (!isUrl.test(response.headers.location)) {
          response.headers.location = url.resolve(self.uri.href, response.headers.location)
        }
        self.uri = response.headers.location
        self.redirects.push(
          { statusCode : response.statusCode
          , redirectUri: response.headers.location 
          }
        )
        delete self.req
        delete self.agent
        delete self._started
        if (self.headers) {
          delete self.headers.host
        }
        if (log) log('Redirect to %uri', self)
        request(self, self.callback)
        return // Ignore the rest of the response
      } else {
        self._redirectsFollowed = self._redirectsFollowed || 0
        // Be a good stream and emit end when the response is finished.
        // Hack to emit end on close because of a core bug that never fires end
        response.on('close', function () {
          if (!self._ended) self.response.emit('end')
        })

        if (self.encoding) {
          if (self.dests.length !== 0) {
            console.error("Ingoring encoding parameter as this stream is being piped to another stream which makes the encoding option invalid.")
          } else {
            response.setEncoding(self.encoding)
          }
        }

        self.pipeDest = function (dest) {
          if (dest.headers) {
            dest.headers['content-type'] = response.headers['content-type']
            if (response.headers['content-length']) {
              dest.headers['content-length'] = response.headers['content-length']
            }
          }
          if (dest.setHeader) {
            for (var i in response.headers) {
              dest.setHeader(i, response.headers[i])
            }
            dest.statusCode = response.statusCode
          }
          if (self.pipefilter) self.pipefilter(response, dest)
        }

        self.dests.forEach(function (dest) {
          self.pipeDest(dest)
        })

        response.on("data", function (chunk) {
          self._destdata = true
          self.emit("data", chunk)
        })
        response.on("end", function (chunk) {
          self._ended = true
          self.emit("end", chunk)
        })
        response.on("close", function () {self.emit("close")})

        self.emit('response', response)

        if (self.onResponse) {
          self.onResponse(null, response)
        }
        if (self.callback) {
          var buffer = []
          var bodyLen = 0
          self.on("data", function (chunk) {
            buffer.push(chunk)
            bodyLen += chunk.length
          })
          self.on("end", function () {
            if (buffer.length && Buffer.isBuffer(buffer[0])) {
              var body = new Buffer(bodyLen)
              var i = 0
              buffer.forEach(function (chunk) {
                chunk.copy(body, i, 0, chunk.length)
                i += chunk.length
              })
              if (self.encoding === null) {
                response.body = body
              } else {
                response.body = body.toString()
              }
            } else if (buffer.length) {
              response.body = buffer.join('')
            }

            if (self.json) {
              try {
                response.body = JSON.parse(response.body)
              } catch (e) {}
            }

            self.callback(null, response, response.body)
          })
        }
      }
    })

    if (self.timeout && !self.timeoutTimer) {
      self.timeoutTimer = setTimeout(function() {
        self.req.abort()
        var e = new Error("ETIMEDOUT")
        e.code = "ETIMEDOUT"
        self.emit("error", e)
      }, self.timeout)
    }
    
    self.req.on('error', clientErrorHandler)
  }

  self.once('pipe', function (src) {
    if (self.ntick) throw new Error("You cannot pipe to this stream after the first nextTick() after creation of the request stream.")
    self.src = src
    if (isReadStream(src)) {
      if (!self.headers['content-type'] && !self.headers['Content-Type'])
        self.headers['content-type'] = mimetypes.lookup(src.path.slice(src.path.lastIndexOf('.')+1))
    } else {
      if (src.headers) {
        for (var i in src.headers) {
          if (!self.headers[i]) {
            self.headers[i] = src.headers[i]
          }
        }
      }
      if (src.method && !self.method) {
        self.method = src.method
      }
    }

    self.on('pipe', function () {
      console.error("You have already piped to this stream. Pipeing twice is likely to break the request.")
    })
  })

  process.nextTick(function () {
    if (self.body) {
      if (Array.isArray(self.body)) {
        self.body.forEach(function(part) {
          self.write(part)
        })
      } else {
        self.write(self.body)
      }
      self.end()
    } else if (self.requestBodyStream) {
      console.warn("options.requestBodyStream is deprecated, please pass the request object to stream.pipe.")
      self.requestBodyStream.pipe(self)
    } else if (!self.src) {
      self.headers['content-length'] = 0
      self.end()
    }
    self.ntick = true
  })
}
Request.prototype.pipe = function (dest) {
  if (this.response) {
    if (this._destdata) {
      throw new Error("You cannot pipe after data has been emitted from the response.")
    } else if (this._ended) {
      throw new Error("You cannot pipe after the response has been ended.")
    } else {
      stream.Stream.prototype.pipe.call(this, dest)
      this.pipeDest(dest)
      return dest
    }
  } else {
    this.dests.push(dest)
    stream.Stream.prototype.pipe.call(this, dest)
    return dest
  }
}
Request.prototype.write = function () {
  if (!this._started) this.start()
  if (!this.req) throw new Error("This request has been piped before http.request() was called.")
  this.req.write.apply(this.req, arguments)
}
Request.prototype.end = function () {
  if (!this._started) this.start()
  if (!this.req) throw new Error("This request has been piped before http.request() was called.")
  this.req.end.apply(this.req, arguments)
}
Request.prototype.pause = function () {
  if (!this.response) throw new Error("This request has been piped before http.request() was called.")
  this.response.pause.apply(this.response, arguments)
}
Request.prototype.resume = function () {
  if (!this.response) throw new Error("This request has been piped before http.request() was called.")
  this.response.resume.apply(this.response, arguments)
}

function request (options, callback) {
  if (typeof options === 'string') options = {uri:options}
  if (callback) options.callback = callback
  var r = new Request(options)
  r.request()
  return r
}

module.exports = request

request.defaults = function (options) {
  var def = function (method) {
    var d = function (opts, callback) {
      if (typeof opts === 'string') opts = {uri:opts}
      for (var i in options) {
        if (opts[i] === undefined) opts[i] = options[i]
      }
      return method(opts, callback)
    }
    return d
  }
  var de = def(request)
  de.get = def(request.get)
  de.post = def(request.post)
  de.put = def(request.put)
  de.head = def(request.head)
  de.del = def(request.del)
  de.cookie = def(request.cookie)
  de.jar = def(request.jar)
  return de
}

request.forever = function (agentOptions, optionsArg) {
  var options = {}
  if (agentOptions) {
    for (option in optionsArg) {
      options[option] = optionsArg[option]
    }
  }
  options.agent = new ForeverAgent(agentOptions)
  return request.defaults(options)
}

request.get = request
request.post = function (options, callback) {
  if (typeof options === 'string') options = {uri:options}
  options.method = 'POST'
  return request(options, callback)
}
request.put = function (options, callback) {
  if (typeof options === 'string') options = {uri:options}
  options.method = 'PUT'
  return request(options, callback)
}
request.head = function (options, callback) {
  if (typeof options === 'string') options = {uri:options}
  options.method = 'HEAD'
  if (options.body || options.requestBodyStream || options.json || options.multipart) {
    throw new Error("HTTP HEAD requests MUST NOT include a request body.")
  }
  return request(options, callback)
}
request.del = function (options, callback) {
  if (typeof options === 'string') options = {uri:options}
  options.method = 'DELETE'
  return request(options, callback)
}
request.jar = function () {
  return new CookieJar
}
request.cookie = function (str) {
  if (typeof str !== 'string') throw new Error("The cookie function only accepts STRING as param")
  return new Cookie(str)
}
