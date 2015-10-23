var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var URI = 'http://localhost:1337'
var PARAMS = {
  auth: {
    token: 'foo'
  },
  scope: 'myorg',
  team: 'myteam'
}

var commands = ['create', 'destroy', 'add', 'rm', 'ls']

test('team create basic', function (t) {
  var teamData = {
    name: PARAMS.team,
    scope_id: 1234,
    created: '2015-07-23T18:07:49.959Z',
    updated: '2015-07-23T18:07:49.959Z',
    deleted: null
  }
  server.expect('PUT', '/-/org/myorg/team', function (req, res) {
    t.equal(req.method, 'PUT')
    onJsonReq(req, function (json) {
      t.same(json, { name: PARAMS.team })
      res.statusCode = 200
      res.json(teamData)
    })
  })
  client.team('create', URI, PARAMS, function (err, data) {
    t.ifError(err, 'no errors')
    t.same(data, teamData)
    t.end()
  })
})

test('team destroy', function (t) {
  var teamData = {
    name: 'myteam',
    scope_id: 1234,
    created: '2015-07-23T18:07:49.959Z',
    updated: '2015-07-23T18:07:49.959Z',
    deleted: '2015-07-23T18:27:27.178Z'
  }
  server.expect('DELETE', '/-/team/myorg/myteam', function (req, res) {
    t.equal(req.method, 'DELETE')
    onJsonReq(req, function (json) {
      t.same(json, undefined)
      res.statusCode = 200
      res.json(teamData)
    })
  })
  client.team('destroy', URI, PARAMS, function (err, data) {
    t.ifError(err, 'no errors')
    t.same(data, teamData)
    t.end()
  })
})

test('team add basic', function (t) {
  var params = Object.create(PARAMS)
  params.user = 'zkat'
  server.expect('PUT', '/-/team/myorg/myteam/user', function (req, res) {
    t.equal(req.method, 'PUT')
    onJsonReq(req, function (json) {
      t.same(json, { user: params.user })
      res.statusCode = 200
      res.json(undefined)
    })
  })
  client.team('add', URI, params, function (err, data) {
    t.ifError(err, 'no errors')
    t.same(data, undefined)
    t.end()
  })
})

test('team add user not in org', function (t) {
  var params = Object.create(PARAMS)
  params.user = 'zkat'
  var errMsg = 'user is already in team'
  server.expect('PUT', '/-/team/myorg/myteam/user', function (req, res) {
    t.equal(req.method, 'PUT')
    res.statusCode = 400
    res.json({
      error: errMsg
    })
  })
  client.team('add', URI, params, function (err, data) {
    t.equal(err.message, errMsg + ' : ' + '-/team/myorg/myteam/user')
    t.same(data, {error: errMsg})
    t.end()
  })
})

test('team rm basic', function (t) {
  var params = Object.create(PARAMS)
  params.user = 'bcoe'
  server.expect('DELETE', '/-/team/myorg/myteam/user', function (req, res) {
    t.equal(req.method, 'DELETE')
    onJsonReq(req, function (json) {
      t.same(json, params)
      res.statusCode = 200
      res.json(undefined)
    })
  })
  client.team('rm', URI, params, function (err, data) {
    t.ifError(err, 'no errors')
    t.same(data, undefined)
    t.end()
  })
})

test('team ls (on org)', function (t) {
  var params = Object.create(PARAMS)
  params.team = null
  var teams = ['myorg:team1', 'myorg:team2', 'myorg:team3']
  server.expect('GET', '/-/org/myorg/team?format=cli', function (req, res) {
    t.equal(req.method, 'GET')
    onJsonReq(req, function (json) {
      t.same(json, undefined)
      res.statusCode = 200
      res.json(teams)
    })
  })
  client.team('ls', URI, params, function (err, data) {
    t.ifError(err, 'no errors')
    t.same(data, teams)
    t.end()
  })
})

test('team ls (on team)', function (t) {
  var uri = '/-/team/myorg/myteam/user?format=cli'
  var users = ['zkat', 'bcoe']
  server.expect('GET', uri, function (req, res) {
    t.equal(req.method, 'GET')
    onJsonReq(req, function (json) {
      t.same(json, undefined)
      res.statusCode = 200
      res.json(users)
    })
  })
  client.team('ls', URI, PARAMS, function (err, data) {
    t.ifError(err, 'no errors')
    t.same(data, users)
    t.end()
  })
})

// test('team edit', function (t) {
//   server.expect('PUT', '/-/org/myorg/team', function (req, res) {
//     t.equal(req.method, 'PUT')
//     res.statusCode = 201
//     res.json({})
//   })
//   client.team('create', URI, PARAMS, function (err, data) {
//     t.ifError(err, 'no errors')
//     t.end()
//   })
// })

test('team command base validation', function (t) {
  t.throws(function () {
    client.team(undefined, URI, PARAMS, nop)
  }, 'command is required')
  commands.forEach(function (cmd) {
    t.throws(function () {
      client.team(cmd, undefined, PARAMS, nop)
    }, 'registry URI is required')
    t.throws(function () {
      client.team(cmd, URI, undefined, nop)
    }, 'params is required')
    t.throws(function () {
      client.team(cmd, URI, {scope: 'o', team: 't'}, nop)
    }, 'auth is required')
    t.throws(function () {
      client.team(cmd, URI, {auth: {token: 'f'}, team: 't'}, nop)
    }, 'scope is required')
    t.throws(function () {
      client.team(cmd, URI, PARAMS, {})
    }, 'callback must be a function')
    if (cmd !== 'ls') {
      t.throws(function () {
        client.team(
          cmd, URI, {auth: {token: 'f'}, scope: 'o'}, nop)
      }, 'team name is required')
    }
    if (cmd === 'add' || cmd === 'rm') {
      t.throws(function () {
        client.team(
          cmd, URI, PARAMS, nop)
      }, 'user is required')
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
