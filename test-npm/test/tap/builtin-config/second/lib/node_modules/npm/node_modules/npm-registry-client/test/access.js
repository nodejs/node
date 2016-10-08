var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var URI = 'http://localhost:1337'
var PARAMS = {
  auth: { token: 'foo' },
  scope: 'myorg',
  team: 'myteam',
  package: '@foo/bar',
  permissions: 'read-write'
}
var UNSCOPED = {
  auth: { token: 'foo' },
  scope: 'myorg',
  team: 'myteam',
  package: 'bar',
  permissions: 'read-write'
}

var commands = [
  'public', 'restricted', 'grant', 'revoke', 'ls-packages', 'ls-collaborators'
]

test('access public', function (t) {
  server.expect('POST', '/-/package/%40foo%2Fbar/access', function (req, res) {
    t.equal(req.method, 'POST')
    onJsonReq(req, function (json) {
      t.deepEqual(json, { access: 'public' })
      res.statusCode = 200
      res.json({ accessChanged: true })
    })
  })
  var params = Object.create(PARAMS)
  params.package = '@foo/bar'
  client.access('public', URI, params, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')
    t.end()
  })
})

test('access restricted', function (t) {
  server.expect('POST', '/-/package/%40foo%2Fbar/access', function (req, res) {
    t.equal(req.method, 'POST')
    onJsonReq(req, function (json) {
      t.deepEqual(json, { access: 'restricted' })
      res.statusCode = 200
      res.json({ accessChanged: true })
    })
  })
  client.access('restricted', URI, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')
    t.end()
  })
})

test('access grant basic', function (t) {
  server.expect('PUT', '/-/team/myorg/myteam/package', function (req, res) {
    t.equal(req.method, 'PUT')
    onJsonReq(req, function (json) {
      t.deepEqual(json, {
        permissions: PARAMS.permissions,
        package: PARAMS.package
      })
      res.statusCode = 201
      res.json({ accessChanged: true })
    })
  })
  client.access('grant', URI, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')
    t.end()
  })
})

test('access grant basic unscoped', function (t) {
  server.expect('PUT', '/-/team/myorg/myteam/package', function (req, res) {
    t.equal(req.method, 'PUT')
    onJsonReq(req, function (json) {
      t.deepEqual(json, {
        permissions: UNSCOPED.permissions,
        package: UNSCOPED.package
      })
      res.statusCode = 201
      res.json({ accessChanged: true })
    })
  })
  client.access('grant', URI, UNSCOPED, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')
    t.end()
  })
})

test('access revoke basic', function (t) {
  server.expect('DELETE', '/-/team/myorg/myteam/package', function (req, res) {
    t.equal(req.method, 'DELETE')
    onJsonReq(req, function (json) {
      t.deepEqual(json, {
        package: PARAMS.package
      })
      res.statusCode = 200
      res.json({ accessChanged: true })
    })
  })
  client.access('revoke', URI, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')
    t.end()
  })
})

test('access revoke basic unscoped', function (t) {
  server.expect('DELETE', '/-/team/myorg/myteam/package', function (req, res) {
    t.equal(req.method, 'DELETE')
    onJsonReq(req, function (json) {
      t.deepEqual(json, {
        package: UNSCOPED.package
      })
      res.statusCode = 200
      res.json({ accessChanged: true })
    })
  })
  client.access('revoke', URI, UNSCOPED, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')
    t.end()
  })
})

test('ls-packages on team', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  var uri = '/-/team/myorg/myteam/package?format=cli'
  server.expect('GET', uri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json(serverPackages)
  })
  client.access('ls-packages', URI, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.same(data, clientPackages)
    t.end()
  })
})

test('ls-packages on org', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  var uri = '/-/org/myorg/package?format=cli'
  server.expect('GET', uri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json(serverPackages)
  })
  var params = Object.create(PARAMS)
  params.team = null
  client.access('ls-packages', URI, params, function (error, data) {
    t.ifError(error, 'no errors')
    t.same(data, clientPackages)
    t.end()
  })
})

test('ls-packages on user', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  var firstUri = '/-/org/myorg/package?format=cli'
  server.expect('GET', firstUri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 404
    res.json({error: 'not found'})
  })
  var secondUri = '/-/user/myorg/package?format=cli'
  server.expect('GET', secondUri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json(serverPackages)
  })
  var params = Object.create(PARAMS)
  params.team = null
  client.access('ls-packages', URI, params, function (error, data) {
    t.ifError(error, 'no errors')
    t.same(data, clientPackages)
    t.end()
  })
})

test('ls-collaborators', function (t) {
  var serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read'
  }
  var clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only'
  }
  var uri = '/-/package/%40foo%2Fbar/collaborators?format=cli'
  server.expect('GET', uri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json(serverCollaborators)
  })
  client.access('ls-collaborators', URI, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.same(data, clientCollaborators)
    t.end()
  })
})

test('ls-collaborators w/scope', function (t) {
  var serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read'
  }
  var clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only'
  }
  var uri = '/-/package/%40foo%2Fbar/collaborators?format=cli&user=zkat'
  server.expect('GET', uri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json(serverCollaborators)
  })
  var params = Object.create(PARAMS)
  params.user = 'zkat'
  client.access('ls-collaborators', URI, params, function (error, data) {
    t.ifError(error, 'no errors')
    t.same(data, clientCollaborators)
    t.end()
  })
})

test('ls-collaborators w/o scope', function (t) {
  var serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read'
  }
  var clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only'
  }
  var uri = '/-/package/bar/collaborators?format=cli&user=zkat'
  server.expect('GET', uri, function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json(serverCollaborators)
  })
  var params = Object.create(UNSCOPED)
  params.user = 'zkat'
  client.access('ls-collaborators', URI, params, function (error, data) {
    t.ifError(error, 'no errors')
    t.same(data, clientCollaborators)
    t.end()
  })
})

test('access command base validation', function (t) {
  t.throws(function () {
    client.access(undefined, URI, PARAMS, nop)
  }, 'command is required')
  t.throws(function () {
    client.access('whoops', URI, PARAMS, nop)
  }, 'command must be a valid subcommand')
  commands.forEach(function (cmd) {
    t.throws(function () {
      client.access(cmd, undefined, PARAMS, nop)
    }, 'registry URI is required')
    t.throws(function () {
      client.access(cmd, URI, undefined, nop)
    }, 'params is required')
    t.throws(function () {
      client.access(cmd, URI, '', nop)
    }, 'params must be an object')
    t.throws(function () {
      client.access(cmd, URI, {scope: 'o', team: 't'}, nop)
    }, 'auth is required')
    t.throws(function () {
      client.access(cmd, URI, {auth: 5, scope: 'o', team: 't'}, nop)
    }, 'auth must be an object')
    t.throws(function () {
      client.access(cmd, URI, PARAMS, {})
    }, 'callback must be a function')
    t.throws(function () {
      client.access(cmd, URI, PARAMS, undefined)
    }, 'callback is required')
    if (contains([
      'public', 'restricted'
    ], cmd)) {
      t.throws(function () {
        var params = Object.create(PARAMS)
        params.package = null
        client.access(cmd, URI, params, nop)
      }, 'package is required')
      t.throws(function () {
        var params = Object.create(PARAMS)
        params.package = 'underscore'
        client.access(cmd, URI, params, nop)
      }, 'only scoped packages are allowed')
    }
    if (contains(['grant', 'revoke', 'ls-packages'], cmd)) {
      t.throws(function () {
        var params = Object.create(PARAMS)
        params.scope = null
        client.access(cmd, URI, params, nop)
      }, 'scope is required')
    }
    if (contains(['grant', 'revoke'], cmd)) {
      t.throws(function () {
        var params = Object.create(PARAMS)
        params.team = null
        client.access(cmd, URI, params, nop)
      }, 'team is required')
    }
    if (cmd === 'grant') {
      t.throws(function () {
        var params = Object.create(PARAMS)
        params.permissions = null
        client.access(cmd, URI, params, nop)
      }, 'permissions are required')
      t.throws(function () {
        var params = Object.create(PARAMS)
        params.permissions = 'idkwhat'
        client.access(cmd, URI, params, nop)
      }, 'permissions must be either read-only or read-write')
    }
  })
  t.end()
})

test('cleanup', function (t) {
  server.close()
  t.end()
})

function onJsonReq (req, cb) {
  var buffer = ''
  req.setEncoding('utf8')
  req.on('data', function (data) { buffer += data })
  req.on('end', function () { cb(buffer ? JSON.parse(buffer) : undefined) })
}

function contains (arr, item) {
  return arr.indexOf(item) !== -1
}
