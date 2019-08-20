'use strict'

const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const { test } = require('tap')
const tnock = require('./util/tnock.js')

const team = require('../index.js')

const REG = 'http://localhost:1337'
const OPTS = figgyPudding({})({
  registry: REG
})

test('create', t => {
  tnock(t, REG).put(
    '/-/org/foo/team', { name: 'cli' }
  ).reply(201, { name: 'cli' })
  return team.create('@foo:cli', OPTS).then(ret => {
    t.deepEqual(ret, { name: 'cli' }, 'request succeeded')
  })
})

test('create bad entity name', t => {
  return team.create('go away', OPTS).then(
    () => { throw new Error('should not succeed') },
    err => { t.ok(err, 'error on bad entity name') }
  )
})

test('create empty entity', t => {
  return team.create(undefined, OPTS).then(
    () => { throw new Error('should not succeed') },
    err => { t.ok(err, 'error on bad entity name') }
  )
})

test('create w/ description', t => {
  tnock(t, REG).put('/-/org/foo/team', {
    name: 'cli',
    description: 'just some cool folx'
  }).reply(201, { name: 'cli' })
  return team.create('@foo:cli', OPTS.concat({
    description: 'just some cool folx'
  })).then(ret => {
    t.deepEqual(ret, { name: 'cli' }, 'no desc in return')
  })
})

test('destroy', t => {
  tnock(t, REG).delete(
    '/-/team/foo/cli'
  ).reply(204, {})
  return team.destroy('@foo:cli', OPTS).then(ret => {
    t.deepEqual(ret, {}, 'request succeeded')
  })
})

test('add', t => {
  tnock(t, REG).put(
    '/-/team/foo/cli/user', { user: 'zkat' }
  ).reply(201, {})
  return team.add('zkat', '@foo:cli', OPTS).then(ret => {
    t.deepEqual(ret, {}, 'request succeeded')
  })
})

test('rm', t => {
  tnock(t, REG).delete(
    '/-/team/foo/cli/user', { user: 'zkat' }
  ).reply(204, {})
  return team.rm('zkat', '@foo:cli', OPTS).then(ret => {
    t.deepEqual(ret, {}, 'request succeeded')
  })
})

test('lsTeams', t => {
  tnock(t, REG).get(
    '/-/org/foo/team?format=cli'
  ).reply(200, ['foo:bar', 'foo:cli'])
  return team.lsTeams('foo', OPTS).then(ret => {
    t.deepEqual(ret, ['foo:bar', 'foo:cli'], 'got teams')
  })
})

test('lsTeams error', t => {
  tnock(t, REG).get(
    '/-/org/foo/team?format=cli'
  ).reply(500)
  return team.lsTeams('foo', OPTS).then(
    () => { throw new Error('should not succeed') },
    err => { t.equal(err.code, 'E500', 'got error code') }
  )
})

test('lsTeams.stream', t => {
  tnock(t, REG).get(
    '/-/org/foo/team?format=cli'
  ).reply(200, ['foo:bar', 'foo:cli'])
  return getStream.array(team.lsTeams.stream('foo', OPTS)).then(ret => {
    t.deepEqual(ret, ['foo:bar', 'foo:cli'], 'got teams')
  })
})

test('lsUsers', t => {
  tnock(t, REG).get(
    '/-/team/foo/cli/user?format=cli'
  ).reply(500)
  return team.lsUsers('@foo:cli', OPTS).then(
    () => { throw new Error('should not succeed') },
    err => { t.equal(err.code, 'E500', 'got error code') }
  )
})

test('lsUsers error', t => {
  tnock(t, REG).get(
    '/-/team/foo/cli/user?format=cli'
  ).reply(200, ['iarna', 'zkat'])
  return team.lsUsers('@foo:cli', OPTS).then(ret => {
    t.deepEqual(ret, ['iarna', 'zkat'], 'got team members')
  })
})

test('lsUsers.stream', t => {
  tnock(t, REG).get(
    '/-/team/foo/cli/user?format=cli'
  ).reply(200, ['iarna', 'zkat'])
  return getStream.array(team.lsUsers.stream('@foo:cli', OPTS)).then(ret => {
    t.deepEqual(ret, ['iarna', 'zkat'], 'got team members')
  })
})

test('edit', t => {
  t.throws(() => {
    team.edit()
  }, /not implemented/)
  t.done()
})
