'use strict'

const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const test = require('tap').test
const tnock = require('./util/tnock.js')

const org = require('../index.js')

const OPTS = figgyPudding({registry: {}})({
  registry: 'https://mock.reg/'
})

test('set', t => {
  const memDeets = {
    org: {
      name: 'myorg',
      size: 15
    },
    user: 'myuser',
    role: 'admin'
  }
  tnock(t, OPTS.registry).put('/-/org/myorg/user', {
    user: 'myuser',
    role: 'admin'
  }).reply(201, memDeets)
  return org.set('myorg', 'myuser', 'admin', OPTS).then(res => {
    t.deepEqual(res, memDeets, 'got a membership details object back')
  })
})

test('optional role for set', t => {
  const memDeets = {
    org: {
      name: 'myorg',
      size: 15
    },
    user: 'myuser',
    role: 'developer'
  }
  tnock(t, OPTS.registry).put('/-/org/myorg/user', {
    user: 'myuser'
  }).reply(201, memDeets)
  return org.set('myorg', 'myuser', OPTS).then(res => {
    t.deepEqual(res, memDeets, 'got a membership details object back')
  })
})

test('rm', t => {
  tnock(t, OPTS.registry).delete('/-/org/myorg/user', {
    user: 'myuser'
  }).reply(204)
  return org.rm('myorg', 'myuser', OPTS).then(() => {
    t.ok(true, 'request succeeded')
  })
})

test('ls', t => {
  const roster = {
    'zkat': 'developer',
    'iarna': 'admin',
    'isaacs': 'owner'
  }
  tnock(t, OPTS.registry).get('/-/org/myorg/user').reply(200, roster)
  return org.ls('myorg', OPTS).then(res => {
    t.deepEqual(res, roster, 'got back a roster')
  })
})

test('ls stream', t => {
  const roster = {
    'zkat': 'developer',
    'iarna': 'admin',
    'isaacs': 'owner'
  }
  const rosterArr = Object.keys(roster).map(k => [k, roster[k]])
  tnock(t, OPTS.registry).get('/-/org/myorg/user').reply(200, roster)
  return getStream.array(org.ls.stream('myorg', OPTS)).then(res => {
    t.deepEqual(res, rosterArr, 'got back a roster, in entries format')
  })
})
