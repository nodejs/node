'use strict'

const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const {test} = require('tap')
const tnock = require('./util/tnock.js')

const access = require('../index.js')

const REG = 'http://localhost:1337'
const OPTS = figgyPudding({})({
  registry: REG
})

test('access public', t => {
  tnock(t, REG).post(
    '/-/package/%40foo%2Fbar/access', {access: 'public'}
  ).reply(200)
  return access.public('@foo/bar', OPTS).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access restricted', t => {
  tnock(t, REG).post(
    '/-/package/%40foo%2Fbar/access', {access: 'restricted'}
  ).reply(200)
  return access.restricted('@foo/bar', OPTS).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access 2fa-required', t => {
  tnock(t, REG).post('/-/package/%40foo%2Fbar/access', {
    publish_requires_tfa: true
  }).reply(200, {ok: true})
  return access.tfaRequired('@foo/bar', OPTS).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access 2fa-not-required', t => {
  tnock(t, REG).post('/-/package/%40foo%2Fbar/access', {
    publish_requires_tfa: false
  }).reply(200, {ok: true})
  return access.tfaNotRequired('@foo/bar', OPTS).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access grant basic read-write', t => {
  tnock(t, REG).put('/-/team/myorg/myteam/package', {
    package: '@foo/bar',
    permissions: 'read-write'
  }).reply(201)
  return access.grant(
    '@foo/bar', 'myorg:myteam', 'read-write', OPTS
  ).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access grant basic read-only', t => {
  tnock(t, REG).put('/-/team/myorg/myteam/package', {
    package: '@foo/bar',
    permissions: 'read-only'
  }).reply(201)
  return access.grant(
    '@foo/bar', 'myorg:myteam', 'read-only', OPTS
  ).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access grant bad perm', t => {
  return access.grant(
    '@foo/bar', 'myorg:myteam', 'unknown', OPTS
  ).then(ret => {
    throw new Error('should not have succeeded')
  }, err => {
    t.match(
      err.message,
      /must be.*read-write.*read-only/,
      'only read-write and read-only are accepted'
    )
  })
})

test('access grant no entity', t => {
  return access.grant(
    '@foo/bar', undefined, 'read-write', OPTS
  ).then(ret => {
    throw new Error('should not have succeeded')
  }, err => {
    t.match(
      err.message,
      /Expected string/,
      'passing undefined entity gives useful error'
    )
  })
})

test('access grant basic unscoped', t => {
  tnock(t, REG).put('/-/team/myorg/myteam/package', {
    package: 'bar',
    permissions: 'read-write'
  }).reply(201)
  return access.grant(
    'bar', 'myorg:myteam', 'read-write', OPTS
  ).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access revoke basic', t => {
  tnock(t, REG).delete('/-/team/myorg/myteam/package', {
    package: '@foo/bar'
  }).reply(200)
  return access.revoke('@foo/bar', 'myorg:myteam', OPTS).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('access revoke basic unscoped', t => {
  tnock(t, REG).delete('/-/team/myorg/myteam/package', {
    package: 'bar'
  }).reply(200, {accessChanged: true})
  return access.revoke('bar', 'myorg:myteam', OPTS).then(ret => {
    t.deepEqual(ret, true, 'request succeeded')
  })
})

test('ls-packages on team', t => {
  const serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read',
    '@foo/other': 'shrödinger'
  }
  const clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only',
    '@foo/other': 'shrödinger'
  }
  tnock(t, REG).get(
    '/-/team/myorg/myteam/package?format=cli'
  ).reply(200, serverPackages)
  return access.lsPackages('myorg:myteam', OPTS).then(data => {
    t.deepEqual(data, clientPackages, 'got client package info')
  })
})

test('ls-packages on org', t => {
  const serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read',
    '@foo/other': 'shrödinger'
  }
  const clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only',
    '@foo/other': 'shrödinger'
  }
  tnock(t, REG).get(
    '/-/org/myorg/package?format=cli'
  ).reply(200, serverPackages)
  return access.lsPackages('myorg', OPTS).then(data => {
    t.deepEqual(data, clientPackages, 'got client package info')
  })
})

test('ls-packages on user', t => {
  const serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read',
    '@foo/other': 'shrödinger'
  }
  const clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only',
    '@foo/other': 'shrödinger'
  }
  const srv = tnock(t, REG)
  srv.get('/-/org/myuser/package?format=cli').reply(404, {error: 'not found'})
  srv.get('/-/user/myuser/package?format=cli').reply(200, serverPackages)
  return access.lsPackages('myuser', OPTS).then(data => {
    t.deepEqual(data, clientPackages, 'got client package info')
  })
})

test('ls-packages error on team', t => {
  tnock(t, REG).get('/-/team/myorg/myteam/package?format=cli').reply(404)
  return access.lsPackages('myorg:myteam', OPTS).then(
    () => { throw new Error('should not have succeeded') },
    err => t.equal(err.code, 'E404', 'spit out 404 directly if team provided')
  )
})

test('ls-packages error on user', t => {
  const srv = tnock(t, REG)
  srv.get('/-/org/myuser/package?format=cli').reply(404, {error: 'not found'})
  srv.get('/-/user/myuser/package?format=cli').reply(404, {error: 'not found'})
  return access.lsPackages('myuser', OPTS).then(
    () => { throw new Error('should not have succeeded') },
    err => t.equal(err.code, 'E404', 'spit out 404 if both reqs fail')
  )
})

test('ls-packages bad response', t => {
  tnock(t, REG).get(
    '/-/team/myorg/myteam/package?format=cli'
  ).reply(200, JSON.stringify(null))
  return access.lsPackages('myorg:myteam', OPTS).then(data => {
    t.deepEqual(data, null, 'succeeds with null')
  })
})

test('ls-packages stream', t => {
  const serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read',
    '@foo/other': 'shrödinger'
  }
  const clientPackages = [
    ['@foo/bar', 'read-write'],
    ['@foo/util', 'read-only'],
    ['@foo/other', 'shrödinger']
  ]
  tnock(t, REG).get(
    '/-/team/myorg/myteam/package?format=cli'
  ).reply(200, serverPackages)
  return getStream.array(
    access.lsPackages.stream('myorg:myteam', OPTS)
  ).then(data => {
    t.deepEqual(data, clientPackages, 'got streamed client package info')
  })
})

test('ls-collaborators', t => {
  const serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read',
    'myorg:thirdteam': 'special-case'
  }
  const clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only',
    'myorg:thirdteam': 'special-case'
  }
  tnock(t, REG).get(
    '/-/package/%40foo%2Fbar/collaborators?format=cli'
  ).reply(200, serverCollaborators)
  return access.lsCollaborators('@foo/bar', OPTS).then(data => {
    t.deepEqual(data, clientCollaborators, 'got collaborators')
  })
})

test('ls-collaborators stream', t => {
  const serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read',
    'myorg:thirdteam': 'special-case'
  }
  const clientCollaborators = [
    ['myorg:myteam', 'read-write'],
    ['myorg:anotherteam', 'read-only'],
    ['myorg:thirdteam', 'special-case']
  ]
  tnock(t, REG).get(
    '/-/package/%40foo%2Fbar/collaborators?format=cli'
  ).reply(200, serverCollaborators)
  return getStream.array(
    access.lsCollaborators.stream('@foo/bar', OPTS)
  ).then(data => {
    t.deepEqual(data, clientCollaborators, 'got collaborators')
  })
})

test('ls-collaborators w/scope', t => {
  const serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read',
    'myorg:thirdteam': 'special-case'
  }
  const clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only',
    'myorg:thirdteam': 'special-case'
  }
  tnock(t, REG).get(
    '/-/package/%40foo%2Fbar/collaborators?format=cli&user=zkat'
  ).reply(200, serverCollaborators)
  return access.lsCollaborators('@foo/bar', 'zkat', OPTS).then(data => {
    t.deepEqual(data, clientCollaborators, 'got collaborators')
  })
})

test('ls-collaborators w/o scope', t => {
  const serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read',
    'myorg:thirdteam': 'special-case'
  }
  const clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only',
    'myorg:thirdteam': 'special-case'
  }
  tnock(t, REG).get(
    '/-/package/bar/collaborators?format=cli&user=zkat'
  ).reply(200, serverCollaborators)
  return access.lsCollaborators('bar', 'zkat', OPTS).then(data => {
    t.deepEqual(data, clientCollaborators, 'got collaborators')
  })
})

test('ls-collaborators bad response', t => {
  tnock(t, REG).get(
    '/-/package/%40foo%2Fbar/collaborators?format=cli'
  ).reply(200, JSON.stringify(null))
  return access.lsCollaborators('@foo/bar', null, OPTS).then(data => {
    t.deepEqual(data, null, 'succeeds with null')
  })
})

test('error on non-registry specs', t => {
  const resolve = () => { throw new Error('should not succeed') }
  const reject = err => t.match(
    err.message, /spec.*must be a registry spec/, 'registry spec required'
  )
  return Promise.all([
    access.public('foo/bar').then(resolve, reject),
    access.restricted('foo/bar').then(resolve, reject),
    access.grant('foo/bar', 'myorg', 'myteam', 'read-only').then(resolve, reject),
    access.revoke('foo/bar', 'myorg', 'myteam').then(resolve, reject),
    access.lsCollaborators('foo/bar').then(resolve, reject),
    access.tfaRequired('foo/bar').then(resolve, reject),
    access.tfaNotRequired('foo/bar').then(resolve, reject)
  ])
})

test('edit', t => {
  t.equal(typeof access.edit, 'function', 'access.edit exists')
  t.throws(() => {
    access.edit()
  }, /Not implemented/, 'directly throws NIY message')
  t.done()
})
