// Copyright 2010-2012 Mikeal Rogers
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
  , tunnel = require('./tunnel')
  , aws = require('./aws')
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
  https.Agent.prototype._getConnection = function (host, port, cb) {
    var s = tls.connect(port, host, this.options, function () {
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
  Object.keys(obj).forEach(function (i) {
    o[i] = obj[i]
  })
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
  
  var reserved = Object.keys(Request.prototype)
  for (var i in options) {
    if (reserved.indexOf(i) === -1) {
      this[i] = options[i]
    } else {
      if (typeof options[i] === 'function') {
        delete options[i]
      }
    }
  }
  options = copy(options)
  
  this.init(options)
}
util.inherits(Request, stream.Stream)
Request.prototype.init = function (options) {
  var self = this
  
  if (!options) options = {}
  if (process.env.NODE_DEBUG && /request/.test(process.env.NODE_DEBUG)) console.error('REQUEST', options)
  if (!self.pool && self.pool !== false) self.pool = globalPool
  self.dests = []
  self.__isRequestRequest = true
  
  // Protect against double callback
  if (!self._callback && self.callback) {
    self._callback = self.callback
    self.callback = function () {
      if (self._callbackCalled) return // Print a warning maybe?
      self._callback.apply(self, arguments)
      self._callbackCalled = true
    }
    self.on('error', self.callback.bind())
    self.on('complete', self.callback.bind(self, null))
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

    // do the HTTP CONNECT dance using koichik/node-tunnel
    if (http.globalAgent && self.uri.protocol === "https:") {
      var tunnelFn = self.proxy.protocol === "http:"
                   ? tunnel.httpsOverHttp : tunnel.httpsOverHttps

      var tunnelOptions = { proxy: { host: self.proxy.hostname
                                   , port: +self.proxy.port
                                   , proxyAuth: self.proxy.auth }
                          , ca: this.ca }

      self.agent = tunnelFn(tunnelOptions)
      self.tunnel = true
    }
  }

  if (!self.uri.host || !self.uri.pathname) {
    // Invalid URI: it may generate lot of bad errors, like "TypeError: Cannot call method 'indexOf' of undefined" in CookieJar
    // Detect and reject it as soon as possible
    var faultyUri = url.format(self.uri)
    var message = 'Invalid URI "' + faultyUri + '"'
    if (Object.keys(options).length === 0) {
      // No option ? This can be the sign of a redirect
      // As this is a case where the user cannot do anything (he didn't call request directly with this URL)
      // he should be warned that it can be caused by a redirection (can save some hair)
      message += '. This can be caused by a crappy redirection.'
    }
    self.emit('error', new Error(message))
    return // This error was fatal
  }

  self._redirectsFollowed = self._redirectsFollowed || 0
  self.maxRedirects = (self.maxRedirects !== undefined) ? self.maxRedirects : 10
  self.followRedirect = (self.followRedirect !== undefined) ? self.followRedirect : true
  self.followAllRedirects = (self.followAllRedirects !== undefined) ? self.followAllRedirects : false;
  if (self.followRedirect || self.followAllRedirects)
    self.redirects = self.redirects || []

  self.headers = self.headers ? copy(self.headers) : {}

  self.setHost = false
  if (!self.headers.host) {
    self.headers.host = self.uri.hostname
    if (self.uri.port) {
      if ( !(self.uri.port === 80 && self.uri.protocol === 'http:') &&
           !(self.uri.port === 443 && self.uri.protocol === 'https:') )
      self.headers.host += (':'+self.uri.port)
    }
    self.setHost = true
  }
  
  self.jar(self._jar || options.jar)

  if (!self.uri.pathname) {self.uri.pathname = '/'}
  if (!self.uri.port) {
    if (self.uri.protocol == 'http:') {self.uri.port = 80}
    else if (self.uri.protocol == 'https:') {self.uri.port = 443}
  }

  if (self.proxy && !self.tunnel) {
    self.port = self.proxy.port
    self.host = self.proxy.hostname
  } else {
    self.port = self.uri.port
    self.host = self.uri.hostname
  }

  self.clientErrorHandler = function (error) {
    if (self._aborted) return
    
    if (self.setHost) delete self.headers.host
    if (self.req._reusedSocket && error.code === 'ECONNRESET'
        && self.agent.addRequestNoreuse) {
      self.agent = { addRequest: self.agent.addRequestNoreuse.bind(self.agent) }
      self.start()
      self.req.end()
      return
    }
    if (self.timeout && self.timeoutTimer) {
      clearTimeout(self.timeoutTimer)
      self.timeoutTimer = null
    }
    self.emit('error', error)
  }

  self._parserErrorHandler = function (error) {
    if (this.res) {
      if (this.res.request) {
        this.res.request.emit('error', error)
      } else {
        this.res.emit('error', error)
      }
    } else {
      this._httpMessage.emit('error', error)
    }
  }

  if (options.form) {
    self.form(options.form)
  }

  if (options.oauth) {
    self.oauth(options.oauth)
  }
  
  if (options.aws) {
    self.aws(options.aws)
  }

  if (self.uri.auth && !self.headers.authorization) {
    self.headers.authorization = "Basic " + toBase64(self.uri.auth.split(':').map(function(item){ return qs.unescape(item)}).join(':'))
  }
  if (self.proxy && self.proxy.auth && !self.headers['proxy-authorization'] && !self.tunnel) {
    self.headers['proxy-authorization'] = "Basic " + toBase64(self.proxy.auth.split(':').map(function(item){ return qs.unescape(item)}).join(':'))
  }

  if (options.qs) self.qs(options.qs)

  if (self.uri.path) {
    self.path = self.uri.path
  } else {
    self.path = self.uri.pathname + (self.uri.search || "")
  }

  if (self.path.length === 0) self.path = '/'

  if (self.proxy && !self.tunnel) self.path = (self.uri.protocol + '//' + self.uri.host + self.path)

  if (options.json) {
    self.json(options.json)
  } else if (options.multipart) {
    self.boundary = uuid()
    self.multipart(options.multipart)
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

  var protocol = self.proxy && !self.tunnel ? self.proxy.protocol : self.uri.protocol
    , defaultModules = {'http:':http, 'https:':https}
    , httpModules = self.httpModules || {}
    ;
  self.httpModule = httpModules[protocol] || defaultModules[protocol]

  if (!self.httpModule) throw new Error("Invalid protocol")

  if (options.ca) self.ca = options.ca

  if (!self.agent) {
    if (options.agentOptions) self.agentOptions = options.agentOptions

    if (options.agentClass) {
      self.agentClass = options.agentClass
    } else if (options.forever) {
      self.agentClass = protocol === 'http:' ? ForeverAgent : ForeverAgent.SSL
    } else {
      self.agentClass = self.httpModule.Agent
    }
  }

  if (self.pool === false) {
    self.agent = false
  } else {
    self.agent = self.agent || self.getAgent()
    if (self.maxSockets) {
      // Don't use our pooling if node has the refactored client
      self.agent.maxSockets = self.maxSockets
    }
    if (self.pool.maxSockets) {
      // Don't use our pooling if node has the refactored client
      self.agent.maxSockets = self.pool.maxSockets
    }
  }

  self.once('pipe', function (src) {
    if (self.ntick && self._started) throw new Error("You cannot pipe to this stream after the outbound request has started.")
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
    if (self._aborted) return
    
    if (self.body) {
      if (Array.isArray(self.body)) {
        self.body.forEach(function (part) {
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
      if (self.method !== 'GET' && typeof self.method !== 'undefined') {
        self.headers['content-length'] = 0;
      }
      self.end();
    }
    self.ntick = true
  })
}

// Must call this when following a redirect from https to http or vice versa
// Attempts to keep everything as identical as possible, but update the
// httpModule, Tunneling agent, and/or Forever Agent in use.
Request.prototype._updateProtocol = function () {
  var self = this
  var protocol = self.uri.protocol

  if (protocol === 'https:') {
    // previously was doing http, now doing https
    // if it's https, then we might need to tunnel now.
    if (self.proxy) {
      self.tunnel = true
      var tunnelFn = self.proxy.protocol === 'http:'
                   ? tunnel.httpsOverHttp : tunnel.httpsOverHttps
      var tunnelOptions = { proxy: { host: self.proxy.hostname
                                   , post: +self.proxy.port
                                   , proxyAuth: self.proxy.auth }
                          , ca: self.ca }
      self.agent = tunnelFn(tunnelOptions)
      return
    }

    self.httpModule = https
    switch (self.agentClass) {
      case ForeverAgent:
        self.agentClass = ForeverAgent.SSL
        break
      case http.Agent:
        self.agentClass = https.Agent
        break
      default:
        // nothing we can do.  Just hope for the best.
        return
    }

    // if there's an agent, we need to get a new one.
    if (self.agent) self.agent = self.getAgent()

  } else {
    if (log) log('previously https, now http')
    // previously was doing https, now doing http
    // stop any tunneling.
    if (self.tunnel) self.tunnel = false
    self.httpModule = http
    switch (self.agentClass) {
      case ForeverAgent.SSL:
        self.agentClass = ForeverAgent
        break
      case https.Agent:
        self.agentClass = http.Agent
        break
      default:
        // nothing we can do.  just hope for the best
        return
    }

    // if there's an agent, then get a new one.
    if (self.agent) {
      self.agent = null
      self.agent = self.getAgent()
    }
  }
}

Request.prototype.getAgent = function () {
  var Agent = this.agentClass
  var options = {}
  if (this.agentOptions) {
    for (var i in this.agentOptions) {
      options[i] = this.agentOptions[i]
    }
  }
  if (this.ca) options.ca = this.ca

  var poolKey = ''

  // different types of agents are in different pools
  if (Agent !== this.httpModule.Agent) {
    poolKey += Agent.name
  }

  if (!this.httpModule.globalAgent) {
    // node 0.4.x
    options.host = this.host
    options.port = this.port
    if (poolKey) poolKey += ':'
    poolKey += this.host + ':' + this.port
  }

  // ca option is only relevant if proxy or destination are https
  var proxy = this.proxy
  if (typeof proxy === 'string') proxy = url.parse(proxy)
  var caRelevant = (proxy && proxy.protocol === 'https:') || this.uri.protocol === 'https:'
  if (options.ca && caRelevant) {
    if (poolKey) poolKey += ':'
    poolKey += options.ca
  }

  if (!poolKey && Agent === this.httpModule.Agent && this.httpModule.globalAgent) {
    // not doing anything special.  Use the globalAgent
    return this.httpModule.globalAgent
  }

  // we're using a stored agent.  Make sure it's protocol-specific
  poolKey = this.uri.protocol + poolKey

  // already generated an agent for this setting
  if (this.pool[poolKey]) return this.pool[poolKey]

  return this.pool[poolKey] = new Agent(options)
}

Request.prototype.start = function () {
  var self = this

  if (self._aborted) return

  self._started = true
  self.method = self.method || 'GET'
  self.href = self.uri.href
  if (log) log('%method %href', self)

  if (self.src && self.src.stat && self.src.stat.size) {
    self.headers['content-length'] = self.src.stat.size
  }
  if (self._aws) {
    self.aws(self._aws, true)
  }

  self.req = self.httpModule.request(self, function (response) {
    if (response.connection.listeners('error').indexOf(self._parserErrorHandler) === -1) {
      response.connection.once('error', self._parserErrorHandler)
    }
    if (self._aborted) return
    if (self._paused) response.pause()

    self.response = response
    response.request = self
    response.toJSON = toJSON

    if (self.httpModule === https &&
        self.strictSSL &&
        !response.client.authorized) {
      var sslErr = response.client.authorizationError
      self.emit('error', new Error('SSL Error: '+ sslErr))
      return
    }

    if (self.setHost) delete self.headers.host
    if (self.timeout && self.timeoutTimer) {
      clearTimeout(self.timeoutTimer)
      self.timeoutTimer = null
    }  

    var addCookie = function (cookie) {
      if (self._jar) self._jar.add(new Cookie(cookie))
      else cookieJar.add(new Cookie(cookie))
    }

    if (response.headers['set-cookie'] && (!self._disableCookies)) {
      if (Array.isArray(response.headers['set-cookie'])) response.headers['set-cookie'].forEach(addCookie)
      else addCookie(response.headers['set-cookie'])
    }

    if (response.statusCode >= 300 && response.statusCode < 400  &&
        (self.followAllRedirects ||
         (self.followRedirect && (self.method !== 'PUT' && self.method !== 'POST' && self.method !== 'DELETE'))) &&
        response.headers.location) {
      if (self._redirectsFollowed >= self.maxRedirects) {
        self.emit('error', new Error("Exceeded maxRedirects. Probably stuck in a redirect loop."))
        return
      }
      self._redirectsFollowed += 1

      if (!isUrl.test(response.headers.location)) {
        response.headers.location = url.resolve(self.uri.href, response.headers.location)
      }

      var uriPrev = self.uri
      self.uri = url.parse(response.headers.location)

      // handle the case where we change protocol from https to http or vice versa
      if (self.uri.protocol !== uriPrev.protocol) {
        self._updateProtocol()
      }

      self.redirects.push(
        { statusCode : response.statusCode
        , redirectUri: response.headers.location 
        }
      )
      if (self.followAllRedirects) self.method = 'GET'
      // self.method = 'GET'; // Force all redirects to use GET || commented out fixes #215
      delete self.req
      delete self.agent
      delete self._started
      delete self.body
      if (self.headers) {
        delete self.headers.host
      }
      if (log) log('Redirect to %uri', self)
      self.init()
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

      if (self.callback) {
        var buffer = []
        var bodyLen = 0
        self.on("data", function (chunk) {
          buffer.push(chunk)
          bodyLen += chunk.length
        })
        self.on("end", function () {
          if (self._aborted) return
          
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
              response.body = body.toString(self.encoding)
            }
          } else if (buffer.length) {
            response.body = buffer.join('')
          }

          if (self._json) {
            try {
              response.body = JSON.parse(response.body)
            } catch (e) {}
          }
          
          self.emit('complete', response, response.body)
        })
      }
    }
  })

  if (self.timeout && !self.timeoutTimer) {
    self.timeoutTimer = setTimeout(function () {
      self.req.abort()
      var e = new Error("ETIMEDOUT")
      e.code = "ETIMEDOUT"
      self.emit("error", e)
    }, self.timeout)
    
    // Set additional timeout on socket - in case if remote
    // server freeze after sending headers
    if (self.req.setTimeout) { // only works on node 0.6+
      self.req.setTimeout(self.timeout, function () {
        if (self.req) {
          self.req.abort()
          var e = new Error("ESOCKETTIMEDOUT")
          e.code = "ESOCKETTIMEDOUT"
          self.emit("error", e)
        }
      })
    }
  }
  
  self.req.on('error', self.clientErrorHandler)
  self.req.on('drain', function() {
    self.emit('drain')
  })
  self.on('end', function() {
    self.req.connection.removeListener('error', self._parserErrorHandler)
  })
  self.emit('request', self.req)
}

Request.prototype.abort = function () {
  this._aborted = true;
  
  if (this.req) {
    this.req.abort()
  }
  else if (this.response) {
    this.response.abort()
  }
  
  this.emit("abort")
}

Request.prototype.pipeDest = function (dest) {
  var response = this.response
  // Called after the response is received
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
  if (this.pipefilter) this.pipefilter(response, dest)
}

// Composable API
Request.prototype.setHeader = function (name, value, clobber) {
  if (clobber === undefined) clobber = true
  if (clobber || !this.headers.hasOwnProperty(name)) this.headers[name] = value
  else this.headers[name] += ',' + value
  return this
}
Request.prototype.setHeaders = function (headers) {
  for (var i in headers) {this.setHeader(i, headers[i])}
  return this
}
Request.prototype.qs = function (q, clobber) {
  var base
  if (!clobber && this.uri.query) base = qs.parse(this.uri.query)
  else base = {}
  
  for (var i in q) {
    base[i] = q[i]
  }
  
  this.uri = url.parse(this.uri.href.split('?')[0] + '?' + qs.stringify(base))
  this.url = this.uri
  
  return this
}
Request.prototype.form = function (form) {
  this.headers['content-type'] = 'application/x-www-form-urlencoded; charset=utf-8'
  this.body = qs.stringify(form).toString('utf8')
  return this
}
Request.prototype.multipart = function (multipart) {
  var self = this
  self.body = []

  if (!self.headers['content-type']) {
    self.headers['content-type'] = 'multipart/related; boundary=' + self.boundary;
  } else {
    self.headers['content-type'] = self.headers['content-type'].split(';')[0] + '; boundary=' + self.boundary;
  }

  if (!multipart.forEach) throw new Error('Argument error, options.multipart.')

  if (self.preambleCRLF) {
    self.body.push(new Buffer('\r\n'))
  }
  
  multipart.forEach(function (part) {
    var body = part.body
    if(body == null) throw Error('Body attribute missing in multipart.')
    delete part.body
    var preamble = '--' + self.boundary + '\r\n'
    Object.keys(part).forEach(function (key) {
      preamble += key + ': ' + part[key] + '\r\n'
    })
    preamble += '\r\n'
    self.body.push(new Buffer(preamble))
    self.body.push(new Buffer(body))
    self.body.push(new Buffer('\r\n'))
  })
  self.body.push(new Buffer('--' + self.boundary + '--'))
  return self
}
Request.prototype.json = function (val) {
  this.setHeader('content-type', 'application/json')
  this.setHeader('accept', 'application/json')
  this._json = true
  if (typeof val === 'boolean') {
    if (typeof this.body === 'object') this.body = JSON.stringify(this.body)
  } else {
    this.body = JSON.stringify(val)
  }
  return this
}
Request.prototype.aws = function (opts, now) {
  if (!now) {
    this._aws = opts
    return this
  }
  var date = new Date()
  this.setHeader('date', date.toUTCString())
  this.setHeader('authorization', aws.authorization(
    { key: opts.key
    , secret: opts.secret
    , verb: this.method
    , date: date
    , resource: aws.canonicalizeResource('/' + opts.bucket + this.path)
    , contentType: this.headers['content-type'] || ''
    , md5: this.headers['content-md5'] || ''
    , amazonHeaders: aws.canonicalizeHeaders(this.headers)
    }
  ))
  
  return this
}

Request.prototype.oauth = function (_oauth) {
  var form
  if (this.headers['content-type'] && 
      this.headers['content-type'].slice(0, 'application/x-www-form-urlencoded'.length) ===
        'application/x-www-form-urlencoded' 
     ) {
    form = qs.parse(this.body)
  }
  if (this.uri.query) {
    form = qs.parse(this.uri.query)
  } 
  if (!form) form = {}
  var oa = {}
  for (var i in form) oa[i] = form[i]
  for (var i in _oauth) oa['oauth_'+i] = _oauth[i]
  if (!oa.oauth_version) oa.oauth_version = '1.0'
  if (!oa.oauth_timestamp) oa.oauth_timestamp = Math.floor( (new Date()).getTime() / 1000 ).toString()
  if (!oa.oauth_nonce) oa.oauth_nonce = uuid().replace(/-/g, '')
  
  oa.oauth_signature_method = 'HMAC-SHA1'
  
  var consumer_secret = oa.oauth_consumer_secret
  delete oa.oauth_consumer_secret
  var token_secret = oa.oauth_token_secret
  delete oa.oauth_token_secret
  
  var baseurl = this.uri.protocol + '//' + this.uri.host + this.uri.pathname
  var signature = oauth.hmacsign(this.method, baseurl, oa, consumer_secret, token_secret)
  
  // oa.oauth_signature = signature
  for (var i in form) {
    if ( i.slice(0, 'oauth_') in _oauth) {
      // skip 
    } else {
      delete oa['oauth_'+i]
      delete oa[i]
    }
  }
  this.headers.Authorization = 
    'OAuth '+Object.keys(oa).sort().map(function (i) {return i+'="'+oauth.rfc3986(oa[i])+'"'}).join(',')
  this.headers.Authorization += ',oauth_signature="'+oauth.rfc3986(signature)+'"'
  return this
}
Request.prototype.jar = function (jar) {
  var cookies
  
  if (this._redirectsFollowed === 0) {
    this.originalCookieHeader = this.headers.cookie
  }
  
  if (jar === false) {
    // disable cookies
    cookies = false;
    this._disableCookies = true;
  } else if (jar) {
    // fetch cookie from the user defined cookie jar
    cookies = jar.get({ url: this.uri.href })
  } else {
    // fetch cookie from the global cookie jar
    cookies = cookieJar.get({ url: this.uri.href })
  }
  
  if (cookies && cookies.length) {
    var cookieString = cookies.map(function (c) {
      return c.name + "=" + c.value
    }).join("; ")

    if (this.originalCookieHeader) {
      // Don't overwrite existing Cookie header
      this.headers.cookie = this.originalCookieHeader + '; ' + cookieString
    } else {
      this.headers.cookie = cookieString
    }
  }
  this._jar = jar
  return this
}


// Stream API
Request.prototype.pipe = function (dest, opts) {
  if (this.response) {
    if (this._destdata) {
      throw new Error("You cannot pipe after data has been emitted from the response.")
    } else if (this._ended) {
      throw new Error("You cannot pipe after the response has been ended.")
    } else {
      stream.Stream.prototype.pipe.call(this, dest, opts)
      this.pipeDest(dest)
      return dest
    }
  } else {
    this.dests.push(dest)
    stream.Stream.prototype.pipe.call(this, dest, opts)
    return dest
  }
}
Request.prototype.write = function () {
  if (!this._started) this.start()
  return this.req.write.apply(this.req, arguments)
}
Request.prototype.end = function (chunk) {
  if (chunk) this.write(chunk)
  if (!this._started) this.start()
  this.req.end()
}
Request.prototype.pause = function () {
  if (!this.response) this._paused = true
  else this.response.pause.apply(this.response, arguments)
}
Request.prototype.resume = function () {
  if (!this.response) this._paused = false
  else this.response.resume.apply(this.response, arguments)
}
Request.prototype.destroy = function () {
  if (!this._ended) this.end()
}

// organize params for post, put, head, del
function initParams(uri, options, callback) {
  if ((typeof options === 'function') && !callback) callback = options;
  if (options && typeof options === 'object') {
    options.uri = uri;
  } else if (typeof uri === 'string') {
    options = {uri:uri};
  } else {
    options = uri;
    uri = options.uri;
  }
  return { uri: uri, options: options, callback: callback };
}

function request (uri, options, callback) {
  if (typeof uri === 'undefined') throw new Error('undefined is not a valid uri or options object.')
  if ((typeof options === 'function') && !callback) callback = options;
  if (options && typeof options === 'object') {
    options.uri = uri;
  } else if (typeof uri === 'string') {
    options = {uri:uri};
  } else {
    options = uri;
  }

  if (callback) options.callback = callback;
  var r = new Request(options)
  return r
}

module.exports = request

request.initParams = initParams;

request.defaults = function (options, requester) {
  var def = function (method) {
    var d = function (uri, opts, callback) {
      var params = initParams(uri, opts, callback);
      for (var i in options) {
        if (params.options[i] === undefined) params.options[i] = options[i]
      }
      if(typeof requester === 'function') {
        if(method === request) {
          method = requester;
        } else {
          params.options._requester = requester;
        }
      }
      return method(params.options, params.callback);
    }
    return d;
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
  if (optionsArg) {
    for (option in optionsArg) {
      options[option] = optionsArg[option]
    }
  }
  if (agentOptions) options.agentOptions = agentOptions
  options.forever = true
  return request.defaults(options)
}

request.get = request
request.post = function (uri, options, callback) {
  var params = initParams(uri, options, callback);
  params.options.method = 'POST';
  return request(params.uri || null, params.options, params.callback)
}
request.put = function (uri, options, callback) {
  var params = initParams(uri, options, callback);
  params.options.method = 'PUT'
  return request(params.uri || null, params.options, params.callback)
}
request.head = function (uri, options, callback) {
  var params = initParams(uri, options, callback);
  params.options.method = 'HEAD'
  if (params.options.body || 
      params.options.requestBodyStream || 
      (params.options.json && typeof params.options.json !== 'boolean') || 
      params.options.multipart) {
    throw new Error("HTTP HEAD requests MUST NOT include a request body.")
  }
  return request(params.uri || null, params.options, params.callback)
}
request.del = function (uri, options, callback) {
  var params = initParams(uri, options, callback);
  params.options.method = 'DELETE'
  if(typeof params.options._requester === 'function') {
    request = params.options._requester;
  }
  return request(params.uri || null, params.options, params.callback)
}
request.jar = function () {
  return new CookieJar
}
request.cookie = function (str) {
  if (str && str.uri) str = str.uri
  if (typeof str !== 'string') throw new Error("The cookie function only accepts STRING as param")
  return new Cookie(str)
}

// Safe toJSON

function getSafe (self, uuid) {  
  if (typeof self === 'object' || typeof self === 'function') var safe = {}
  if (Array.isArray(self)) var safe = []

  var recurse = []
  
  Object.defineProperty(self, uuid, {})
  
  var attrs = Object.keys(self).filter(function (i) {
    if (i === uuid) return false 
    if ( (typeof self[i] !== 'object' && typeof self[i] !== 'function') || self[i] === null) return true
    return !(Object.getOwnPropertyDescriptor(self[i], uuid))
  })
  
  
  for (var i=0;i<attrs.length;i++) {
    if ( (typeof self[attrs[i]] !== 'object' && typeof self[attrs[i]] !== 'function') || 
          self[attrs[i]] === null
        ) {
      safe[attrs[i]] = self[attrs[i]]
    } else {
      recurse.push(attrs[i])
      Object.defineProperty(self[attrs[i]], uuid, {})
    }
  }

  for (var i=0;i<recurse.length;i++) {
    safe[recurse[i]] = getSafe(self[recurse[i]], uuid)
  }
  
  return safe
}

function toJSON () {
  return getSafe(this, (((1+Math.random())*0x10000)|0).toString(16))
}

Request.prototype.toJSON = toJSON

