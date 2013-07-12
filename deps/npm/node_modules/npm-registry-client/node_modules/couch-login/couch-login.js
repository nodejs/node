var request = require('request')
, url = require('url')
, crypto = require('crypto')
, YEAR = (1000 * 60 * 60 * 24 * 365)
, BASIC = {}
, assert = require('assert')

module.exports = CouchLogin

function CouchLogin (couch, tok) {
  if (!(this instanceof CouchLogin)) {
    return new CouchLogin(couch)
  }

  if (!couch) throw new Error(
    "Need to pass a couch url to CouchLogin constructor")

  if (couch instanceof CouchLogin)
    couch = couch.couch

  couch = url.parse(couch)
  if (couch.auth) {
    var a = couch.auth.split(':')
    var name = a.shift()
    var password = a.join(':')
    this.name = name
    if (password)
      this.auth = new Buffer(name + ':' + password).toString('base64')
  } else {
    this.auth = null
    this.name = null
  }
  delete couch.auth

  if (tok === 'anonymous')
    tok = NaN
  else if (tok === 'basic')
    tok = BASIC

  this.token = tok
  this.couch = url.format(couch)
  this.proxy = null

  this.maxAge = YEAR

  // replace with a CA cert string, or an array, or leave as null
  // to use the defaults included in node.  Only relevant for HTTPS
  // couches, of course.
  this.ca = null

  // set to boolean true or false to specify the strictSSL behavior.
  // if left as null, then it'll use whatever node defaults to, which
  // is false <=0.8.x, and true >=0.9.x
  //
  // Again, only relevant for https couches, of course.
  this.strictSSL = null
}

CouchLogin.prototype =
{ get: makeReq('GET')
, del: makeReq('DELETE')
, put: makeReq('PUT', true)
, post: makeReq('POST', true)
, login: login
, logout: logout
, decorate: decorate
, changePass: changePass
, signup: signup
, deleteAccount: deleteAccount
, anon: anon
, anonymous: anon
, valid: valid
}

Object.defineProperty(CouchLogin.prototype, 'constructor',
  { value: CouchLogin, enumerable: false })

function decorate (req, res) {
  assert(this instanceof CouchLogin)
  req.couch = res.couch = this

  // backed by some sort of set(k,v,cb), get(k,cb) session storage.
  var session = req.session || res.session || null
  if (session) {
    this.tokenGet = function (cb) {
      session.get('couch_token', cb)
    }

    // don't worry about it failing.  it'll just mean a login next time.
    this.tokenSet = function (tok, cb) {
      session.set('couch_token', tok, cb || function () {})
    }

    this.tokenDel = function (cb) {
      session.del('couch_token', cb || function () {})
    }
  }

  return this
}

function anon () {
  assert(this instanceof CouchLogin)
  return new CouchLogin(this.couch, NaN)
}

function makeReq (meth, body, f) { return function madeReq (p, d, cb) {
  assert(this instanceof CouchLogin)
  f = f || (this.token !== this.token)
  if (!f && !valid(this.token)) {
    // lazily get the token.
    if (this.tokenGet) return this.tokenGet(function (er, tok) {
      if (er || !valid(tok)) {
        if (!body) cb = d, d = null
        return cb(new Error('auth token expired or invalid'))
      }
      this.token = tok
      return madeReq.call(this, p, d, cb)
    }.bind(this))

    // no getter, no token, no business.
    return process.nextTick(function () {
      if (!body) cb = d, d = null
      cb(new Error('auth token expired or invalid'))
    })
  }

  if (!body) cb = d, d = null

  var h = {}
  , u = url.resolve(this.couch, p)
  , req = { uri: u, headers: h, json: true, body: d, method: meth }

  if (this.token === BASIC) {
    if (!this.auth)
      return process.nextTick(cb.bind(this, new Error(
        'Using basic auth and no auth provided')))
    else
      h.authorization = 'Basic ' + this.auth
  } else if (this.token) {
    h.cookie = 'AuthSession=' + this.token.AuthSession
  }

  if (this.proxy) {
    req.proxy = this.proxy
  }

  // we're handling cookies, don't do it for us.
  req.jar = false

  if (this.ca)
    req.ca = this.ca

  if (typeof this.strictSSL === 'boolean')
    req.strictSSL = req.rejectUnauthorized = this.strictSSL

  request(req, function (er, res, data) {
    // update cookie.
    if (er || res.statusCode !== 200) return cb(er, res, data)
    addToken.call(this, res)
    return cb.call(this, er, res, data)
  }.bind(this))
}}

function login (auth, cb) {
  assert(this instanceof CouchLogin)
  if (this.token === BASIC) {
    this.auth = new Buffer(auth.name + ':' + auth.password).toString('base64')
    this.name = auth.name
    cb = cb.bind(this, null, { statusCode: 200 }, { ok: true })
    return process.nextTick(cb)
  }
  var a = { name: auth.name, password: auth.password }
  var req = makeReq('post', true, true)
  req.call(this, '/_session', a, function (er, cr, data) {
    if (er || (cr && cr.statusCode >= 400))
      return cb(er, cr, data)
    this.name = auth.name
    cb(er, cr, data)
  }.bind(this))
}

function changePass (auth, cb) {
  assert(this instanceof CouchLogin)
  if (!auth.name || !auth.password) return cb(new Error('invalid auth'))

  var u = '/_users/org.couchdb.user:' + auth.name
  this.get(u, function (er, res, data) {
    if (er || res.statusCode !== 200) return cb(er, res, data)

    // copy any other keys we're setting here.
    // note that name, password_sha, salt, and date
    // are all set explicitly below.
    Object.keys(auth).filter(function (k) {
      return k.charAt(0) !== '_'
          && k !== 'password'
          && k !== 'password_sha'
          && k !== 'salt'
    }).forEach(function (k) {
      data[k] = auth[k]
    })

    var newSalt = crypto.randomBytes(30).toString('hex')
    , newPass = auth.password
    , newSha = sha(newPass + newSalt)

    data.password_sha = newSha
    data.salt = newSalt
    data.date = new Date().toISOString()

    this.put(u + '?rev=' + data._rev, data, function (er, res, data) {
      if (er || res.statusCode >= 400)
        return cb(er, res, data)
      if (this.name && this.name !== auth.name)
        return cb(er, res, data)
      return this.login(auth, cb)
    }.bind(this))
  }.bind(this))
}

// They said that there should probably be a warning before
// deleting the user's whole account, so here it is:
//
// WATCH OUT!
function deleteAccount (name, cb) {
  assert(this instanceof CouchLogin)
  var u = '/_users/org.couchdb.user:' + name
  this.get(u, thenPut.bind(this))

  function thenPut (er, res, data) {
    if (er || res.statusCode !== 200) {
      return cb(er, res, data)
    }

    // user accts can't be just DELETE'd by non-admins
    // so we take the existing doc and just slap a _deleted
    // flag on it to fake it.  Works the same either way
    // in couch.
    data._deleted = true
    this.put(u + '?rev=' + data._rev, data, cb)
  }
}



function signup (auth, cb) {
  assert(this instanceof CouchLogin)
  if (this.token && this.token !== BASIC) {

    return this.logout(function (er, res, data) {
      if (er || res && res.statusCode !== 200) {
        return cb(er, res, data)
      }

      if (this.token) {
        return cb(new Error('failed to delete token'), res, data)
      }

      this.signup(auth, cb)
    }.bind(this))
  }

  // make a new user record.
  var newSalt = crypto.randomBytes(30).toString('hex')
  , newSha = sha(auth.password + newSalt)
  , user = { _id: 'org.couchdb.user:' + auth.name
           , name: auth.name
           , roles: []
           , type: 'user'
           , password_sha: newSha
           , salt: newSalt
           , date: new Date().toISOString() }

  Object.keys(auth).forEach(function (k) {
    if (k === 'name' || k === 'password') return
    user[k] = auth[k]
  })

  var u = '/_users/' + user._id
  makeReq('put', true, true).call(this, u, user, function (er, res, data) {
    if (er || res.statusCode >= 400) {
      return cb(er, res, data)
    }

    // it worked! log in as that user and get their record
    this.login(auth, function (er, res, data) {
      if (er || (res && res.statusCode >= 400) || data && data.error) {
        return cb(er, res, data)
      }
      this.get(u, cb)
    }.bind(this))
  }.bind(this))
}

function addToken (res) {
  assert(this instanceof CouchLogin)
  // not doing the whole login session cookie thing.
  if (this.token === BASIC)
    return

  // attach the token, if a new one was provided.
  var sc = res.headers['set-cookie']
  if (!sc) return
  if (!Array.isArray(sc)) sc = [sc]

  sc = sc.filter(function (c) {
    return c.match(/^AuthSession=/)
  })[0]

  if (!sc.length) return

  sc = sc.split(/\s*;\s*/).map(function (p) {
    return p.split('=')
  }).reduce(function (set, p) {
    var k = p[0] === 'AuthSession' ? p[0] : p[0].toLowerCase()
    , v = k === 'expires' ? Date.parse(p[1])
        : p[1] === '' || p[1] === undefined ? true // HttpOnly
        : p[1]
    set[k] = v
    return set
  }, {})

  if (sc.hasOwnProperty('max-age')) {
    var ma = sc['max-age']
    sc.expires = (ma <= 0) ? 0 : Date.now() + (ma * 1000)
    delete sc['max-age']
  }

  // expire the session after 1 year, even if couch won't.
  if (!sc.hasOwnProperty('expires')) {
    sc.expires = Date.now() + YEAR
  }

  if (!isNaN(this.maxAge)) {
    sc.expires = Math.min(sc.expires, Date.now() + this.maxAge)
  }

  this.token = sc
  if (this.tokenSet) this.tokenSet(this.token)
}


function logout (cb) {
  assert(this instanceof CouchLogin)
  if (!this.token && this.tokenGet) {
    return this.tokenGet(function (er, tok) {
      if (er || !tok)
        return cb(null, { statusCode: 200 }, {})
      this.token = tok
      this.logout(cb)
    }.bind(this))
  }

  if (!valid(this.token)) {
    this.token = null
    if (this.tokenDel) this.tokenDel()
    return process.nextTick(cb.bind(this, null, { statusCode: 200 }, {}))
  }

  var h = { cookie: 'AuthSession=' + this.token.AuthSession }
  , u = url.resolve(this.couch, '/_session')
  , req = { uri: u, headers: h, json: true }

  request.del(req, function (er, res, data) {
    if (er || (res.statusCode !== 200 && res.statusCode !== 404)) {
      return cb(er, res, data)
    }

    this.token = null
    if (this.tokenDel)
      this.tokenDel()
    cb(er, res, data)
  }.bind(this))
}

function valid (token) {
  if (token === BASIC) return true
  if (!token) return false
  if (!token.hasOwnProperty('expires')) return true
  return token.expires > Date.now()
}

function sha (s) {
  return crypto.createHash("sha1").update(s).digest("hex")
}
