'use strict'
const fetch = require('make-fetch-happen').defaults({retry: false})
const validate = require('aproba')
const url = require('url')
const os = require('os')

exports.adduserCouch = adduserCouch
exports.loginCouch = loginCouch
exports.adduserWeb = adduserWeb
exports.loginWeb = loginWeb
exports.login = login
exports.adduser = adduser
exports.get = get
exports.set = set
exports.listTokens = listTokens
exports.removeToken = removeToken
exports.createToken = createToken

// try loginWeb, catch the "not supported" message and fall back to couch
function login (opener, prompter, conf) {
  validate('FFO', arguments)
  return loginWeb(opener, conf).catch(er => {
    if (er instanceof WebLoginNotSupported) {
      process.emit('log', 'verbose', 'web login not supported, trying couch')
      return prompter(conf.creds)
        .then(data => loginCouch(data.username, data.password, conf))
    } else {
      throw er
    }
  })
}

function adduser (opener, prompter, conf) {
  validate('FFO', arguments)
  return adduserWeb(opener, conf).catch(er => {
    if (er instanceof WebLoginNotSupported) {
      process.emit('log', 'verbose', 'web adduser not supported, trying couch')
      return prompter(conf.creds)
        .then(data => adduserCouch(data.username, data.email, data.password, conf))
    } else {
      throw er
    }
  })
}

function adduserWeb (opener, conf) {
  validate('FO', arguments)
  const body = { create: true }
  process.emit('log', 'verbose', 'web adduser', 'before first POST')
  return webAuth(opener, conf, body)
}

function loginWeb (opener, conf) {
  validate('FO', arguments)
  process.emit('log', 'verbose', 'web login', 'before first POST')
  return webAuth(opener, conf, {})
}

function webAuth (opener, conf, body) {
  if (!conf.opts) conf.opts = {}
  const target = url.resolve(conf.registry, '-/v1/login')
  body.hostname = conf.hostname || os.hostname()
  return fetchJSON({
    target: target,
    method: 'POST',
    body: body,
    opts: conf.opts,
    saveResponse: true
  }).then(result => {
    const res = result[0]
    const content = result[1]
    process.emit('log', 'verbose', 'web auth', 'got response', content)
    const doneUrl = content.doneUrl
    const loginUrl = content.loginUrl
    if (typeof doneUrl !== 'string' ||
        typeof loginUrl !== 'string' ||
        !doneUrl || !loginUrl) {
      throw new WebLoginInvalidResponse('POST', target, res, content)
    }
    process.emit('log', 'verbose', 'web auth', 'opening url pair')
    const doneConf = {
      target: doneUrl,
      method: 'GET',
      opts: conf.opts,
      saveResponse: true
    }
    return opener(loginUrl).then(() => fetchJSON(doneConf)).then(onDone)
    function onDone (result) {
      const res = result[0]
      const content = result[1]
      if (res.status === 200) {
        if (!content.token) {
          throw new WebLoginInvalidResponse('GET', doneUrl, res, content)
        } else {
          return content
        }
      } else if (res.status === 202) {
        const retry = +res.headers.get('retry-after')
        if (retry > 0) {
          return new Promise(resolve => setTimeout(resolve, 1000 * retry))
            .then(() => fetchJSON(doneConf)).then(onDone)
        } else {
          return fetchJSON(doneConf).then(onDone)
        }
      } else {
        throw new WebLoginInvalidResponse('GET', doneUrl, res, content)
      }
    }
  }).catch(er => {
    if (er.statusCode >= 400 && er.statusCode <= 499) {
      throw new WebLoginNotSupported('POST', target, {
        status: er.statusCode,
        headers: { raw: () => er.headers }
      }, er.body)
    } else {
      throw er
    }
  })
}

function adduserCouch (username, email, password, conf) {
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
    .then(result => {
      result.username = username
      return result
    })
}

function loginCouch (username, password, conf) {
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
    if (err.code === 'E400') {
      err.message = `There is no user with the username "${username}".`
      throw err
    }
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
  }).then(result => {
    result.username = username
    return result
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

class HttpErrorGeneral extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    if (body && body.error) {
      this.message = `Registry returned ${this.statusCode} for ${this.method} on ${this.target}: ${body.error}`
    } else {
      this.message = `Registry returned ${this.statusCode} for ${this.method} on ${this.target}`
    }
    Error.captureStackTrace(this, HttpErrorGeneral)
  }
}

class WebLoginInvalidResponse extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'Invalid response from web login endpoint'
    Error.captureStackTrace(this, WebLoginInvalidResponse)
  }
}

class WebLoginNotSupported extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'Web login not supported'
    this.code = 'ENYI'
    Error.captureStackTrace(this, WebLoginNotSupported)
  }
}

class HttpErrorAuthOTP extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'OTP required for authentication'
    this.code = 'EOTP'
    Error.captureStackTrace(this, HttpErrorAuthOTP)
  }
}

class HttpErrorAuthIPAddress extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'Login is not allowed from your IP address'
    this.code = 'EAUTHIP'
    Error.captureStackTrace(this, HttpErrorAuthIPAddress)
  }
}

class HttpErrorAuthUnknown extends HttpErrorBase {
  constructor (method, target, res, body) {
    super(method, target, res, body)
    this.message = 'Unable to authenticate, need: ' + res.headers.get('www-authenticate')
    this.code = 'EAUTHIP'
    Error.captureStackTrace(this, HttpErrorAuthUnknown)
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
  process.emit('log', 'http', 'request', '→', conf.method || 'GET', conf.target)
  return fetch.defaults(conf.opts || {})(conf.target, fetchOpts).catch(err => {
    throw new FetchError(err, conf.method, conf.target)
  }).then(res => {
    if (res.headers.has('npm-notice')) {
      process.emit('warn', 'notice', res.headers.get('npm-notice'))
    }
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
    const retVal = conf.saveResponse ? result : content
    process.emit('log', 'http', res.status, `← ${res.statusText} (${conf.target})`)
    if (res.status === 401 && res.headers.get('www-authenticate')) {
      const auth = res.headers.get('www-authenticate').split(/,\s*/).map(s => s.toLowerCase())
      if (auth.indexOf('ipaddress') !== -1) {
        throw new HttpErrorAuthIPAddress(conf.method, conf.target, res, content)
      } else if (auth.indexOf('otp') !== -1) {
        throw new HttpErrorAuthOTP(conf.method, conf.target, res, content)
      } else {
        throw new HttpErrorAuthUnknown(conf.method, conf.target, res, content)
      }
    } else if (res.status < 200 || res.status >= 300) {
      throw new HttpErrorGeneral(conf.method, conf.target, res, content)
    } else {
      return retVal
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
