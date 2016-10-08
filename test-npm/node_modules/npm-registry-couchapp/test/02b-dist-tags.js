var common = require('./common.js')
var path = require('path')
var url = require('url')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var request = require('request')
var db = 'http://localhost:15986/registry/'
var conf = path.resolve(__dirname, 'fixtures', 'npmrc')
var dturl = url.resolve(reg, '/-/package/package/dist-tags')
var auth = 'Basic ' + new Buffer('user:pass@:!%\'').toString('base64')
var origdt = {
  latest : "0.2.3",
  alpha : "0.2.3-alpha"
}

test('GET dist-tags', function (t) {
  request({ url: dturl, json: true }, function (er, res, body) {
    t.same(body, origdt)
    t.end()
  })
})

test('PUT new dist tag set', function (t) {
  var newdt = {
    "latest" : "0.2.3",
    "alpha" : "0.2.3-alpha",
    "foo": "0.0.2"
  }

  t.plan(3)

  t.test('fail as anon', function (t) {
    request.put({
      url: dturl,
      body: newdt,
      json: true
    }, function (er, res, body) {
      t.equal(res.statusCode, 403)
      t.same(body, {
        error: 'forbidden',
        reason: 'Please log in before writing to the db'
      })
      t.end()
    })
  })

  t.test('succeed with auth', function (t) {
    request.put({
      url: dturl,
      body: newdt,
      headers: {
        authorization: auth
      },
      json: true
    }, function (er, res, body) {
      t.equal(res.statusCode, 201)
      t.same(body, { ok: 'dist-tags updated' })
      t.end()
    })
  })

  t.test('GET after success', function (t) {
    request({url: dturl, json: true }, function (er, res, body) {
      t.same(body, newdt)
      t.end()
    })
  })
})

test('POST new dist tag set', function (t) {
  var newdt = {
    latest : "0.2.3-alpha",
    alpha : "0.2.3-alpha",
    foo: "0.0.2",
    blerb: "0.2.3"
  }

  var post = {
    latest: "0.2.3-alpha",
    blerb: "0.2.3"
  }

  request.post({
    url: dturl,
    body: post,
    headers: {
      authorization: auth
    },
    json: true
  }, function (er, res, body) {
    t.equal(res.statusCode, 201)
    t.same(body, { ok: 'dist-tags updated' })
    request({ url: dturl, json: true }, function (er, res, body) {
      t.same(body, newdt)
      t.end()
    })
  })
})

test('PUT one new tag', function (t) {
  var newdt = {
    latest : "0.2.3-alpha",
    alpha : "0.2.3-alpha",
    foo: "0.0.2",
    blerb: "0.0.2"
  }

  request.put({
    url: dturl + '/blerb',
    body: JSON.stringify(newdt.blerb),
    headers: {
      authorization: auth
    }
  }, function (er, res, body) {
    body = JSON.parse(body)
    t.equal(res.statusCode, 201)
    t.same(body, { ok: 'dist-tags updated' })
    request({ url: dturl, json: true }, function (er, res, body) {
      t.same(body, newdt)
      t.end()
    })
  })
})

test('POST one new tag', function (t) {
  var newdt = {
    latest : "0.2.3-alpha",
    alpha : "0.2.3-alpha",
    foo: "0.0.2",
    blerb: "0.2.3"
  }

  request.post({
    url: dturl + '/blerb',
    body: JSON.stringify(newdt.blerb),
    headers: {
      authorization: auth
    }
  }, function (er, res, body) {
    body = JSON.parse(body)
    t.equal(res.statusCode, 201)
    t.same(body, { ok: 'dist-tags updated' })
    request({ url: dturl, json: true }, function (er, res, body) {
      t.same(body, newdt)
      t.end()
    })
  })
})

test('DELETE blerb tag', function (t) {
  var newdt = {
    latest : "0.2.3-alpha",
    alpha : "0.2.3-alpha",
    foo: "0.0.2"
  }

  request.del({
    url: dturl + '/blerb',
    headers: {
      authorization: auth
    },
    json: true
  }, function (er, res, body) {
    t.equal(res.statusCode, 201)
    t.same(body, { ok: 'dist-tags updated' })
    request({ url: dturl, json: true }, function (er, res, body) {
      t.same(body, newdt)
      t.end()
    })
  })
})

test('Cannot DELETE latest tag', function (t) {
  var newdt = {
    latest : "0.2.3-alpha",
    alpha : "0.2.3-alpha",
    foo: "0.0.2"
  }

  request.del({
    url: dturl + '/latest',
    headers: {
      authorization: auth
    },
    json: true
  }, function (er, res, body) {
    t.equal(res.statusCode, 403)
    t.same(body, { error: 'forbidden', reason: 'must have a "latest" dist-tag' })
    request({ url: dturl, json: true }, function (er, res, body) {
      t.same(body, newdt)
      t.end()
    })
  })
})

test('cannot set tag to invalid version', function (t) {
  request.put({
    url: dturl + '/latest',
    headers: {
      authorization: auth
    },
    body: JSON.stringify('999.999.999')
  }, function (er, res, body) {
    body = JSON.parse(body)
    t.equal(res.statusCode, 403)
    t.same(body, {
      error: 'forbidden',
      reason: 'tag points to invalid version: latest'
    })
    t.end()
  })
})

test('put tags back like we found them', function (t) {
  request.put({
    url: dturl,
    headers: {
      authorization: auth
    },
    json: true,
    body: origdt
  }, function (er, res, body) {
    t.equal(res.statusCode, 201)
    t.same(body, { ok: 'dist-tags updated' })
    t.end()
  })
})
