'use strict'

const figgyPudding = require('figgy-pudding')
const test = require('tap').test
const tnock = require('./util/tnock.js')

const OPTS = figgyPudding({ registry: {} })({
  registry: 'https://mock.reg/'
})

const REG = OPTS.registry
const REV = '72-47f2986bfd8e8b55068b204588bbf484'
const unpub = require('../unpublish.js')

test('basic test', t => {
  const doc = {
    _id: 'foo',
    _rev: REV,
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      }
    }
  }
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(200, doc)
  srv.delete(`/foo/-rev/${REV}`).reply(201)
  return unpub('foo', OPTS).then(ret => {
    t.ok(ret, 'foo was unpublished')
  })
})

test('scoped basic test', t => {
  const doc = {
    _id: '@foo/bar',
    _rev: REV,
    name: '@foo/bar',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: '@foo/bar',
        dist: {
          tarball: `${REG}/@foo/bar/-/foo-1.0.0.tgz`
        }
      }
    }
  }
  const srv = tnock(t, REG)
  srv.get('/@foo%2fbar?write=true').reply(200, doc)
  srv.delete(`/@foo%2fbar/-rev/${REV}`).reply(201)
  return unpub('@foo/bar', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})

test('unpublish specific, last version', t => {
  const doc = {
    _id: 'foo',
    _rev: REV,
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      }
    }
  }
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(200, doc)
  srv.delete(`/foo/-rev/${REV}`).reply(201)
  return unpub('foo@1.0.0', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})

test('unpublish specific version', t => {
  const doc = {
    _id: 'foo',
    _rev: REV,
    _revisions: [1, 2, 3],
    _attachments: [1, 2, 3],
    name: 'foo',
    'dist-tags': {
      latest: '1.0.1'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      },
      '1.0.1': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.1.tgz`
        }
      }
    }
  }
  const postEdit = {
    _id: 'foo',
    _rev: REV,
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      }
    }
  }

  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(200, doc)
  srv.put(`/foo/-rev/${REV}`, postEdit).reply(200)
  srv.get('/foo?write=true').reply(200, postEdit)
  srv.delete(`/foo/-/foo-1.0.1.tgz/-rev/${REV}`).reply(200)
  return unpub('foo@1.0.1', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})

test('404 considered a success', t => {
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(404)
  return unpub('foo', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})

test('non-404 errors', t => {
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(500)
  return unpub('foo', OPTS).then(
    () => { throw new Error('should not have succeeded') },
    err => { t.equal(err.code, 'E500', 'got right error from server') }
  )
})

test('packument with missing versions unpublishes whole thing', t => {
  const doc = {
    _id: 'foo',
    _rev: REV,
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    }
  }
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(200, doc)
  srv.delete(`/foo/-rev/${REV}`).reply(201)
  return unpub('foo@1.0.0', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})

test('packument with missing specific version assumed unpublished', t => {
  const doc = {
    _id: 'foo',
    _rev: REV,
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      }
    }
  }
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(200, doc)
  return unpub('foo@1.0.1', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})

test('unpublish specific version without dist-tag update', t => {
  const doc = {
    _id: 'foo',
    _rev: REV,
    _revisions: [1, 2, 3],
    _attachments: [1, 2, 3],
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      },
      '1.0.1': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.1.tgz`
        }
      }
    }
  }
  const postEdit = {
    _id: 'foo',
    _rev: REV,
    name: 'foo',
    'dist-tags': {
      latest: '1.0.0'
    },
    versions: {
      '1.0.0': {
        name: 'foo',
        dist: {
          tarball: `${REG}/foo/-/foo-1.0.0.tgz`
        }
      }
    }
  }
  const srv = tnock(t, REG)
  srv.get('/foo?write=true').reply(200, doc)
  srv.put(`/foo/-rev/${REV}`, postEdit).reply(200)
  srv.get('/foo?write=true').reply(200, postEdit)
  srv.delete(`/foo/-/foo-1.0.1.tgz/-rev/${REV}`).reply(200)
  return unpub('foo@1.0.1', OPTS).then(() => {
    t.ok(true, 'foo was unpublished')
  })
})
