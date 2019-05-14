'use strict'

const common = require('../common-tap.js')
const test = require('tap').test

test('hook add', (t) => {
  let body
  return common.withServer(server => {
    server.filteringRequestBody(bod => {
      body = JSON.parse(bod)
      t.deepEqual(body, {
        type: 'owner',
        name: 'zkat',
        endpoint: 'https://example.com',
        secret: 'sekrit'
      }, 'request sent correct body')
      return true
    })
      .post('/-/npm/v1/hooks/hook', true)
      .reply(201, {
        name: 'zkat',
        type: 'owner',
        endpoint: 'https://example.com'
      })
    return common.npm([
      'hook', 'add', '~zkat', 'https://example.com', 'sekrit',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.match(
        stdout.trim(),
        /^\+ ~zkat.*https:\/\/example\.com$/,
        'output info about new hook'
      )
    })
  })
})

test('hook add --json', (t) => {
  return common.withServer(server => {
    server
      .filteringRequestBody(() => true)
      .post('/-/npm/v1/hooks/hook', true)
      .reply(201, {
        name: 'npm',
        type: 'scope',
        endpoint: 'https://example.com'
      })
    return common.npm([
      'hook', 'add', '~zkat', 'https://example.com', 'sekrit',
      '--json',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.deepEqual(JSON.parse(stdout), {
        name: 'npm',
        type: 'scope',
        endpoint: 'https://example.com'
      }, 'json response data returned')
    })
  })
})

test('hook rm', t => {
  return common.withServer(server => {
    server
      .delete('/-/npm/v1/hooks/hook/dead%40beef')
      .reply(200, {
        name: 'zkat',
        type: 'owner',
        endpoint: 'https://example.com',
        secret: 'sekrit'
      })
    return common.npm([
      'hook', 'rm', 'dead@beef',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.match(
        stdout.trim(),
        /^- ~zkat.*https:\/\/example\.com$/,
        'output info about new hook'
      )
    })
  })
})

test('hook rm --json', t => {
  return common.withServer(server => {
    server
      .delete('/-/npm/v1/hooks/hook/dead%40beef')
      .reply(200, {
        name: 'zkat',
        type: 'owner',
        endpoint: 'https://example.com',
        secret: 'sekrit'
      })
    return common.npm([
      'hook', 'rm', 'dead@beef',
      '--json',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.deepEqual(JSON.parse(stdout), {
        name: 'zkat',
        type: 'owner',
        endpoint: 'https://example.com',
        secret: 'sekrit'
      }, 'json response data returned')
    })
  })
})

test('hook ls', t => {
  const objects = [
    {id: 'foo', type: 'package', name: '@foo/pkg', endpoint: 'foo.com'},
    {id: 'bar', type: 'owner', name: 'bar', endpoint: 'bar.com'},
    {id: 'baz', type: 'scope', name: 'baz', endpoint: 'baz.com'}
  ]
  return common.withServer(server => {
    server
      .get('/-/npm/v1/hooks?package=%40npm%2Fhooks')
      .reply(200, {objects})
    return common.npm([
      'hook', 'ls', '@npm/hooks',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.match(
        stdout,
        /You have 3 hooks configured/,
        'message about hook count'
      )
      t.match(
        stdout,
        /foo\s+.*\s+@foo\/pkg\s+.*\s+foo\.com/,
        'package displayed as expected'
      )
      t.match(
        stdout,
        /bar\s+.*\s+~bar\s+.*\s+bar\.com/,
        'owner displayed as expected'
      )
      t.match(
        stdout,
        /baz\s+.*\s+@baz\s+.*\s+baz\.com/,
        'scope displayed as expected'
      )
    })
  })
})

test('hook ls --json', t => {
  const objects = [
    {id: 'foo'},
    {id: 'bar'},
    {id: 'baz'}
  ]
  return common.withServer(server => {
    server
      .get('/-/npm/v1/hooks?package=%40npm%2Fhooks')
      .reply(200, {objects})
    return common.npm([
      'hook', 'ls', '@npm/hooks',
      '--json',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.deepEqual(JSON.parse(stdout), objects, 'objects output as json')
    })
  })
})

test('hook update', t => {
  return common.withServer(server => {
    server.filteringRequestBody(() => true)
      .put('/-/npm/v1/hooks/hook/dead%40beef', true)
      .reply(200, {
        type: 'scope',
        name: 'npm',
        endpoint: 'https://example.com',
        secret: 'sekrit'
      })
    return common.npm([
      'hook', 'update', 'dead@beef', 'https://example.com', 'sekrit',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      t.match(
        stdout.trim(),
        /^\+ @npm\s+.*\s+https:\/\/example\.com$/,
        'output info about updated hook'
      )
    })
  })
})

test('hook update --json', t => {
  let body
  return common.withServer(server => {
    server.filteringRequestBody(bod => {
      body = JSON.parse(bod)
      t.deepEqual(body, {
        endpoint: 'https://example.com',
        secret: 'sekrit'
      }, 'request sent correct body')
      return true
    })
      .put('/-/npm/v1/hooks/hook/dead%40beef', true)
      .reply(200, {
        endpoint: 'https://example.com',
        secret: 'sekrit'
      })
    return common.npm([
      'hook', 'update', 'dead@beef', 'https://example.com', 'sekrit',
      '--json',
      '--registry', server.registry
    ], {}).then(([code, stdout, stderr]) => {
      t.comment(stdout)
      t.comment(stderr)
      t.equal(code, 0, 'exited successfully')
      const json = JSON.parse(stdout)
      t.deepEqual(json, {
        endpoint: 'https://example.com',
        secret: 'sekrit'
      }, 'json response data returned')
    })
  })
})
