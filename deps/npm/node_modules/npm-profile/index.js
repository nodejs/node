'use strict'
const fetch = require('make-fetch-happen').defaults({retry: false})
const validate = require('aproba')
const url = require('url')

exports.adduser = adduser
exports.login = login
exports.get = get
exports.set = set
exports.listTokens = listTokens
exports.removeToken = removeToken
exports.createToken = createToken

function adduser (username, email, password, conf) {
  validate('SSSO', arguments)
  if (!conf.opts) conf.opts = {}
  const userobj = {
    _id: 'org.couchdb.user:' + username,
    name: username,
    password: password,
    email: email,
    type: 'user',
    roles: [],
    date: new Date().toISOString()
  }
  const logObj = {}
  Object.keys(userobj).forEach(k => {
    logObj[k] = k === 'password' ? 'XXXXX' : userobj[k]
  })
  process.emit('log', 'verbose', 'adduser', 'before first PUT', logObj)

  const target = url.resolve(conf.registry, '-/user/org.couchdb.user:' + encodeURIComponent(username))

  return fetchJSON({target: target, method: 'PUT', body: userobj, opts: conf.opts})
}

function login (username, password, conf) {
  validate('SSO', arguments)
  const userobj = {
    _id: 'org.couchdb.user:' + username,
    name: username,
    password: password,
    type: 'user',
    roles: [],
    date: new Date().toISOString()
  }
  const logObj = {}
  Object.keys(userobj).forEach(k => {
    logObj[k] = k === 'password' ? 'XXXXX' : userobj[k]
  })
  process.emit('log', 'verbose', 'login', 'before first PUT', logObj)

  const target = url.resolve(conf.registry, '-/user/org.couchdb.user:' + encodeURIComponent(username))
  return fetchJSON(Object.assign({method: 'PUT', target: target, body: userobj}, conf)).catch(err => {
    if (err.code === 'E400') err.message = `There is no user with the username "${username}".`
    if (err.code !== 'E409') throw err
    return fetchJSON(Object.assign({method: 'GET', target: target + '?write=true'}, conf)).then(result => {
      Object.keys(result).forEach(function (k) {
        if (!userobj[k] || k === 'roles') {
          userobj[k] = result[k]
        }
      })
      const req = {
        method: 'PUT',
        target: target + '/-rev/' + userobj._rev,
        body: userobj,
        auth: {
          basic: {
            username: username,
            password: password
          }
        }
      }
      return fetchJSON(Object.assign({}, conf, req))
    })
  })
}

function get (conf) {
  validate('O', arguments)
  const target = url.resolve(conf.registry, '-/npm/v1/user')
  return fetchJSON(Object.assign({target: target}, conf))
}

function set (profile, conf) {
  validate('OO', arguments)
  const target = url.resolve(conf.registry, '-/npm/v1/user')
  Object.keys(profile).forEach(key => {
    // profile keys can't be empty strings, but they CAN be null
    if (profile[key] === '') profile[key] = null
  })
  return fetchJSON(Object.assign({target: target, method: 'POST', body: profile}, conf))
}

function listTokens (conf) {
  validate('O', arguments)

  return untilLastPage(`-/npm/v1/tokens`)

  function untilLastPage (href, objects) {
    return fetchJSON(Object.assign({target: url.resolve(conf.registry, href)}, conf)).then(result => {
      objects = objects ? objects.concat(result.objects) : result.objects
      if (result.urls.next) {
        return untilLastPage(result.urls.next, objects)
      } else {
        return objects
      }
    })
  }
}

function removeToken (tokenKey, conf) {
  validate('SO', arguments)
  const target = url.resolve(conf.registry, `-/npm/v1/tokens/token/${tokenKey}`)
  return fetchJSON(Object.assign({target: target, method: 'DELETE'}, conf))
}

function createToken (password, readonly, cidrs, conf) {
  validate('SBAO', arguments)
  const target = url.resolve(conf.registry, '-/npm/v1/tokens')
  const props = {
    password: password,
    readonly: readonly,
    cidr_whitelist: cidrs
  }
  return fetchJSON(Object.assign({target: target, method: 'POST', body: props}, conf))
}

function FetchError (err, method, target) {
  err.method = method
  err.href = target
  return err
}

class HttpErrorBase extends Error {
  constructor (method, target, res, body) {
    super()
    this.headers = res.headers.raw()
    this.statusCode = res.status
    this.code = 'E' + res.status
    this.method = method
    this.target = target
    this.body = body
    this.pkgid = packageName(target)
  }
}

class General extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = `Registry returned ${this.statusCode} for ${this.method} on ${this.href}`
  }
}

class AuthOTP extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'OTP required for authentication'
    this.code = 'EOTP'
    Error.captureStackTrace(this, AuthOTP)
  }
}

class AuthIPAddress extends HttpErrorBase {
  constructor (res, body) {
    super(method, target, res, body)
    this.message = 'Login is not allowed from your IP address'
    this.code = 'EAUTHIP'
    Error.captureStackTrace(this, AuthIPAddress)
  }
}

class AuthUnknown extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'Unable to authenticate, need: ' + res.headers.get('www-authenticate')
    this.code = 'EAUTHIP'
    Error.captureStackTrace(this, AuthIPAddress)
  }
}

function authHeaders (auth) {
  const headers = {}
  if (!auth) return headers
  if (auth.otp) headers['npm-otp'] = auth.otp
  if (auth.token) {
    headers['Authorization'] = 'Bearer ' + auth.token
  } else if (auth.basic) {
    const basic = auth.basic.username + ':' + auth.basic.password
    headers['Authorization'] = 'Basic ' + Buffer.from(basic).toString('base64')
  }
  return headers
}

function fetchJSON (conf) {
  const fetchOpts = {
    method: conf.method,
    headers: Object.assign({}, conf.headers || (conf.auth && authHeaders(conf.auth)) || {})
  }
  if (conf.body != null) {
    fetchOpts.headers['Content-Type'] = 'application/json'
    fetchOpts.body = JSON.stringify(conf.body)
  }
  process.emit('log', 'http', 'request', '→',conf.method || 'GET', conf.target)
  return fetch.defaults(conf.opts || {})(conf.target, fetchOpts).catch(err => {
    throw new FetchError(err, conf.method, conf.target)
  }).then(res => {
    if (res.headers.get('content-type') === 'application/json') {
      return res.json().then(content => [res, content])
    } else {
      return res.buffer().then(content => {
        try {
          return [res, JSON.parse(content)]
        } catch (_) {
          return [res, content]
        }
      })
    }
  }).then(result => {
    const res = result[0]
    const content = result[1]
    process.emit('log', 'http', res.status, `← ${res.statusText} (${conf.target})`)
    if (res.status === 401 && res.headers.get('www-authenticate')) {
      const auth = res.headers.get('www-authenticate').split(/,\s*/).map(s => s.toLowerCase())
      if (auth.indexOf('ipaddress') !== -1) {
        throw new AuthIPAddress(conf.method, conf.target, res, content)
      } else if (auth.indexOf('otp') !== -1) {
        throw new AuthOTP(conf.method, conf.target, res, content)
      } else {
        throw new AuthUnknown(conf.method, conf.target, res, content)
      }
    } else if (res.status < 200 || res.status >= 300) {
      if (typeof content === 'object' && content.error) {
        return content
      } else {
        throw new General(conf.method, conf.target, res, content)
      }
    } else {
      return content
    }
  })
}

function packageName (href) {
  try {
    let basePath = url.parse(href).pathname.substr(1)
    if (!basePath.match(/^-/)) {
      basePath = basePath.split('/')
      var index = basePath.indexOf('_rewrite')
      if (index === -1) {
        index = basePath.length - 1
      } else {
        index++
      }
      return decodeURIComponent(basePath[index])
    }
  } catch (_) {
    // this is ok
  }
}