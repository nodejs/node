'use strict'

const fetch = require('npm-registry-fetch')
const { HttpErrorBase } = require('npm-registry-fetch/lib/errors')
const EventEmitter = require('events')
const os = require('os')
const { URL } = require('url')
const log = require('proc-log')

// try loginWeb, catch the "not supported" message and fall back to couch
const login = (opener, prompter, opts = {}) => {
  const { creds } = opts
  return loginWeb(opener, opts).catch(er => {
    if (er instanceof WebLoginNotSupported) {
      log.verbose('web login not supported, trying couch')
      return prompter(creds)
        .then(data => loginCouch(data.username, data.password, opts))
    } else {
      throw er
    }
  })
}

const adduser = (opener, prompter, opts = {}) => {
  const { creds } = opts
  return adduserWeb(opener, opts).catch(er => {
    if (er instanceof WebLoginNotSupported) {
      log.verbose('web adduser not supported, trying couch')
      return prompter(creds)
        .then(data => adduserCouch(data.username, data.email, data.password, opts))
    } else {
      throw er
    }
  })
}

const adduserWeb = (opener, opts = {}) => {
  log.verbose('web adduser', 'before first POST')
  return webAuth(opener, opts, { create: true })
}

const loginWeb = (opener, opts = {}) => {
  log.verbose('web login', 'before first POST')
  return webAuth(opener, opts, {})
}

const isValidUrl = u => {
  try {
    return /^https?:$/.test(new URL(u).protocol)
  } catch (er) {
    return false
  }
}

const webAuth = (opener, opts, body) => {
  const { hostname } = opts
  body.hostname = hostname || os.hostname()
  const target = '/-/v1/login'
  return fetch(target, {
    ...opts,
    method: 'POST',
    body,
    headers: {
      'npm-use-webauthn': opts.authType === 'webauthn',
    },
  }).then(res => {
    return Promise.all([res, res.json()])
  }).then(([res, content]) => {
    const { doneUrl, loginUrl } = content
    log.verbose('web auth', 'got response', content)
    if (!isValidUrl(doneUrl) || !isValidUrl(loginUrl)) {
      throw new WebLoginInvalidResponse('POST', res, content)
    }
    return content
  }).then(({ doneUrl, loginUrl }) => {
    log.verbose('web auth', 'opening url pair')

    const doneEmitter = new EventEmitter()

    const openPromise = opener(loginUrl, doneEmitter)
    const webAuthCheckPromise = webAuthCheckLogin(doneUrl, { ...opts, cache: false })
      .then(authResult => {
        log.verbose('web auth', 'done-check finished')

        // cancel open prompt if it's present
        doneEmitter.emit('abort')

        return authResult
      })

    return Promise.all([openPromise, webAuthCheckPromise]).then(
      // pick the auth result and pass it along
      ([, authResult]) => authResult
    )
  }).catch(er => {
    if ((er.statusCode >= 400 && er.statusCode <= 499) || er.statusCode === 500) {
      throw new WebLoginNotSupported('POST', {
        status: er.statusCode,
        headers: { raw: () => er.headers },
      }, er.body)
    } else {
      throw er
    }
  })
}

const webAuthCheckLogin = (doneUrl, opts) => {
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

const adduserCouch = (username, email, password, opts = {}) => {
  const body = {
    _id: 'org.couchdb.user:' + username,
    name: username,
    password: password,
    email: email,
    type: 'user',
    roles: [],
    date: new Date().toISOString(),
  }
  const logObj = {
    ...body,
    password: 'XXXXX',
  }
  log.verbose('adduser', 'before first PUT', logObj)

  const target = '/-/user/org.couchdb.user:' + encodeURIComponent(username)
  return fetch.json(target, {
    ...opts,
    method: 'PUT',
    body,
  }).then(result => {
    result.username = username
    return result
  })
}

const loginCouch = (username, password, opts = {}) => {
  const body = {
    _id: 'org.couchdb.user:' + username,
    name: username,
    password: password,
    type: 'user',
    roles: [],
    date: new Date().toISOString(),
  }
  const logObj = {
    ...body,
    password: 'XXXXX',
  }
  log.verbose('login', 'before first PUT', logObj)

  const target = '/-/user/org.couchdb.user:' + encodeURIComponent(username)
  return fetch.json(target, {
    ...opts,
    method: 'PUT',
    body,
  }).catch(err => {
    if (err.code === 'E400') {
      err.message = `There is no user with the username "${username}".`
      throw err
    }
    if (err.code !== 'E409') {
      throw err
    }
    return fetch.json(target, {
      ...opts,
      query: { write: true },
    }).then(result => {
      Object.keys(result).forEach(k => {
        if (!body[k] || k === 'roles') {
          body[k] = result[k]
        }
      })
      const { otp } = opts
      return fetch.json(`${target}/-rev/${body._rev}`, {
        ...opts,
        method: 'PUT',
        body,
        forceAuth: {
          username,
          password: Buffer.from(password, 'utf8').toString('base64'),
          otp,
        },
      })
    })
  }).then(result => {
    result.username = username
    return result
  })
}

const get = (opts = {}) => fetch.json('/-/npm/v1/user', opts)

const set = (profile, opts = {}) => {
  Object.keys(profile).forEach(key => {
    // profile keys can't be empty strings, but they CAN be null
    if (profile[key] === '') {
      profile[key] = null
    }
  })
  return fetch.json('/-/npm/v1/user', {
    ...opts,
    method: 'POST',
    body: profile,
  })
}

const listTokens = (opts = {}) => {
  const untilLastPage = (href, objects) => {
    return fetch.json(href, opts).then(result => {
      objects = objects ? objects.concat(result.objects) : result.objects
      if (result.urls.next) {
        return untilLastPage(result.urls.next, objects)
      } else {
        return objects
      }
    })
  }
  return untilLastPage('/-/npm/v1/tokens')
}

const removeToken = (tokenKey, opts = {}) => {
  const target = `/-/npm/v1/tokens/token/${tokenKey}`
  return fetch(target, {
    ...opts,
    method: 'DELETE',
    ignoreBody: true,
  }).then(() => null)
}

const createToken = (password, readonly, cidrs, opts = {}) => {
  return fetch.json('/-/npm/v1/tokens', {
    ...opts,
    method: 'POST',
    body: {
      password: password,
      readonly: readonly,
      cidr_whitelist: cidrs,
    },
  })
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

const sleep = (ms) =>
  new Promise((resolve, reject) => setTimeout(resolve, ms))

module.exports = {
  adduserCouch,
  loginCouch,
  adduserWeb,
  loginWeb,
  login,
  adduser,
  get,
  set,
  listTokens,
  removeToken,
  createToken,
  webAuthCheckLogin,
}
