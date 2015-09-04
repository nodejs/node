var fs = require('fs')
var path = require('path')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')

var test = require('tap').test
var common = require('../common-tap.js')

var server

test('setup', function (t) {
  mr({port: common.port}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('team create basic', function (t) {
  var teamData = {
    name: 'test',
    scope_id: 1234,
    created: '2015-07-23T18:07:49.959Z',
    updated: '2015-07-23T18:07:49.959Z',
    deleted: null
  }
  server.put('/-/org/myorg/team', JSON.stringify({
    name: teamData.name
  })).reply(200, teamData)
  common.npm([
    'team', 'create', 'myorg:' + teamData.name,
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm team')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr, '', 'no error output')
    t.same(JSON.parse(stdout), teamData)
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
  server.delete('/-/team/myorg/' + teamData.name).reply(200, teamData)
  common.npm([
    'team', 'destroy', 'myorg:' + teamData.name,
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm team')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr, '', 'no error output')
    t.same(JSON.parse(stdout), teamData)
    t.end()
  })
})

test('team add', function (t) {
  var user = 'zkat'
  server.put('/-/team/myorg/myteam/user', JSON.stringify({
    user: user
  })).reply(200)
  common.npm([
    'team', 'add', 'myorg:myteam', user,
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm team')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr, '', 'no error output')
    t.end()
  })
})

test('team rm', function (t) {
  var user = 'zkat'
  server.delete('/-/team/myorg/myteam/user', JSON.stringify({
    user: user
  })).reply(200)
  common.npm([
    'team', 'rm', 'myorg:myteam', user,
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm team')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr, '', 'no error output')
    t.end()
  })
})

test('team ls (on org)', function (t) {
  var teams = ['myorg:team1', 'myorg:team2', 'myorg:team3']
  server.get('/-/org/myorg/team?format=cli').reply(200, teams)
  common.npm([
    'team', 'ls', 'myorg',
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm team')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr, '', 'no error output')
    t.same(JSON.parse(stdout), teams)
    t.end()
  })
})

test('team ls (on team)', function (t) {
  var users = ['zkat', 'bcoe']
  server.get('/-/team/myorg/myteam/user?format=cli').reply(200, users)
  common.npm([
    'team', 'ls', 'myorg:myteam',
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm team')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr, '', 'no error output')
    t.same(JSON.parse(stdout), users)
    t.end()
  })
})

test('cleanup', function (t) {
  t.pass('cleaned up')
  server.done()
  server.close()
  t.end()
})
