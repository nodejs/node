'use strict'

const fetch = require('npm-registry-fetch')
const {HttpErrorBase} = require('npm-registry-fetch/errors.js')
const os = require('os')
const pudding = require('figgy-pudding')
const validate = require('aproba')

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

const ProfileConfig = pudding({
  creds: {},
  hostname: {},
  otp: {}
})

// try loginWeb, catch the "not supported" message and fall back to couch
function login (opener, prompter, opts) {
  validate('FFO', arguments)
  opts = ProfileConfig(opts)
  return loginWeb(opener, opts).catch(er => {
    if (er instanceof WebLoginNotSupported) {
      process.emit('log', 'verbose', 'web login not supported, trying couch')
      return prompter(opts.creds)
        .then(data => loginCouch(data.username, data.password, opts))
    } else {
      throw er
    }
  })
}

function adduser (opener, prompter, opts) {
  validate('FFO', arguments)
  opts = ProfileConfig(opts)
  return adduserWeb(opener, opts).catch(er => {
    if (er instanceof WebLoginNotSupported) {
      process.emit('log', 'verbose', 'web adduser not supported, trying couch')
      return prompter(opts.creds)
        .then(data => adduserCouch(data.username, data.email, data.password, opts))
    } else {
      throw er
    }
  })
}

function adduserWeb (opener, opts) {
  validate('FO', arguments)
  const body = { create: true }
  process.emit('log', 'verbose', 'web adduser', 'before first POST')
  return webAuth(opener, opts, body)
}

function loginWeb (opener, opts) {
  validate('FO', arguments)
  process.emit('log', 'verbose', 'web login', 'before first POST')
  return webAuth(opener, opts, {})
}

function webAuth (opener, opts, body) {
  opts = ProfileConfig(opts)
  body.hostname = opts.hostname || os.hostname()
  const target = '/-/v1/login'
  return fetch(target, opts.concat({
    method: 'POST',
    body
  })).then(res => {
    return Promise.all([res, res.json()])
  }).then(([res, content]) => {
    const {doneUrl, loginUrl} = content
    process.emit('log', 'verbose', 'web auth', 'got response', content)
    if (
      typeof doneUrl !== 'string' ||
      typeof loginUrl !== 'string' ||
      !doneUrl ||
      !loginUrl
    ) {
      throw new WebLoginInvalidResponse('POST', res, content)
    }
    return content
  }).then(({doneUrl, loginUrl}) => {
    process.emit('log', 'verbose', 'web auth', 'opening url pair')
    return opener(loginUrl).then(
      () => webAuthCheckLogin(doneUrl, opts.concat({cache: false}))
    )
  }).catch(er => {
    if ((er.statusCode >= 400 && er.statusCode <= 499) || er.statusCode === 500) {
      throw new WebLoginNotSupported('POST', {
        status: er.statusCode,
        headers: { raw: () => er.headers }
      }, er.body)
    } else {
      throw er
    }
  })
}

function webAuthCheckLogin (doneUrl, opts) {
  return fetch(doneUrl, opts).then(res => {
    return Promise.all([res, res.json()])
  }).then(([res, content]) => {
    if (res.status === 200) {
      if (!content.token) {
        throw new WebLoginInvalidResponse('GET', res, content)
      } else {
        return content
      }
    } else if (res.status === 202) {
      const retry = +res.headers.get('retry-after') * 1000
      if (retry > 0) {
        return sleep(retry).then(() => webAuthCheckLogin(doneUrl, opts))
      } else {
        return webAuthCheckLogin(doneUrl, opts)
      }
    } else {
      throw new WebLoginInvalidResponse('GET', res, content)
    }
  })
}

function adduserCouch (username, email, password, opts) {
  validate('SSSO', arguments)
  opts = ProfileConfig(opts)
  const body = {
    _id: 'org.couchdb.user:' + username,
    name: username,
    password: password,
    email: email,
    type: 'user',
    roles: [],
    date: new Date().toISOString()
  }
  const logObj = {}
  Object.keys(body).forEach(k => {
    logObj[k] = k === 'password' ? 'XXXXX' : body[k]
  })
  process.emit('log', 'verbose', 'adduser', 'before first PUT', logObj)

  const target = '/-/user/org.couchdb.user:' + encodeURIComponent(username)
  return fetch.json(target, opts.concat({
    method: 'PUT',
    body
  })).then(result => {
    result.username = username
    return result
  })
}

function loginCouch (username, password, opts) {
  validate('SSO', arguments)
  opts = ProfileConfig(opts)
  const body = {
    _id: 'org.couchdb.user:' + username,
    name: username,
    password: password,
    type: 'user',
    roles: [],
    date: new Date().toISOString()
  }
  const logObj = {}
  Object.keys(body).forEach(k => {
    logObj[k] = k === 'password' ? 'XXXXX' : body[k]
  })
  process.emit('log', 'verbose', 'login', 'before first PUT', logObj)

  const target = '-/user/org.couchdb.user:' + encodeURIComponent(username)
  return fetch.json(target, opts.concat({
    method: 'PUT',
    body
  })).catch(err => {
    if (err.code === 'E400') {
      err.message = `There is no user with the username "${username}".`
      throw err
    }
    if (err.code !== 'E409') throw err
    return fetch.json(target, opts.concat({
      query: {write: true}
    })).then(result => {
      Object.keys(result).forEach(function (k) {
        if (!body[k] || k === 'roles') {
          body[k] = result[k]
        }
      })
      return fetch.json(`${target}/-rev/${body._rev}`, opts.concat({
        method: 'PUT',
        body,
        forceAuth: {
          username,
          password: Buffer.from(password, 'utf8').toString('base64'),
          otp: opts.otp
        }
      }))
    })
  }).then(result => {
    result.username = username
    return result
  })
}

function get (opts) {
  validate('O', arguments)
  return fetch.json('/-/npm/v1/user', opts)
}

function set (profile, opts) {
  validate('OO', arguments)
  Object.keys(profile).forEach(key => {
    // profile keys can't be empty strings, but they CAN be null
    if (profile[key] === '') profile[key] = null
  })
  return fetch.json('/-/npm/v1/user', ProfileConfig(opts, {
    method: 'POST',
    body: profile
  }))
}

function listTokens (opts) {
  validate('O', arguments)
  opts = ProfileConfig(opts)

  return untilLastPage('/-/npm/v1/tokens')

  function untilLastPage (href, objects) {
    return fetch.json(href, opts).then(result => {
      objects = objects ? objects.concat(result.objects) : result.objects
      if (result.urls.next) {
        return untilLastPage(result.urls.next, objects)
      } else {
        return objects
      }
    })
  }
}

function removeToken (tokenKey, opts) {
  validate('SO', arguments)
  const target = `/-/npm/v1/tokens/token/${tokenKey}`
  return fetch(target, ProfileConfig(opts, {
    method: 'DELETE',
    ignoreBody: true
  })).then(() => null)
}

function createToken (password, readonly, cidrs, opts) {
  validate('SBAO', arguments)
  return fetch.json('/-/npm/v1/tokens', ProfileConfig(opts, {
    method: 'POST',
    body: {
      password: password,
      readonly: readonly,
      cidr_whitelist: cidrs
    }
  }))
}

class WebLoginInvalidResponse extends HttpErrorBase {
  constructor (method, res, body) {
    super(method, res, body)
    this.message = 'Invalid response from web login endpoint'
    Error.captureStackTrace(this, WebLoginInvalidResponse)
  }
}

class WebLoginNotSupported extends HttpErrorBase {
  constructor (method, res, body) {
    super(method, res, body)
    this.message = 'Web login not supported'
    this.code = 'ENYI'
    Error.captureStackTrace(this, WebLoginNotSupported)
  }
}

function sleep (ms) {
  return new Promise((resolve, reject) => setTimeout(resolve, ms))
}
