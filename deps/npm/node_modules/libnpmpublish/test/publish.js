'use strict'

const crypto = require('crypto')
const cloneDeep = require('lodash.clonedeep')
const figgyPudding = require('figgy-pudding')
const mockTar = require('./util/mock-tarball.js')
const { PassThrough } = require('stream')
const ssri = require('ssri')
const { test } = require('tap')
const tnock = require('./util/tnock.js')

const publish = require('../publish.js')

const OPTS = figgyPudding({ registry: {} })({
  registry: 'https://mock.reg/'
})

const REG = OPTS.registry

test('basic publish', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    return publish(manifest, tarData, OPTS.concat({
      token: 'deadbeef'
    })).then(ret => {
      t.ok(ret, 'publish succeeded')
    })
  })
})

test('scoped publish', t => {
  const manifest = {
    name: '@zkat/libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: '@zkat/libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: '@zkat/libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: '@zkat/libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          name: '@zkat/libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/@zkat/libnpmpublish/-/@zkat/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        '@zkat/libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/@zkat%2flibnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      token: 'deadbeef'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('retry after a conflict', t => {
  const REV = '72-47f2986bfd8e8b55068b204588bbf484'
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const basePackument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {},
      versions: {},
      _attachments: {}
    }
    const currentPackument = cloneDeep(Object.assign({}, basePackument, {
      time: {
        modified: new Date().toISOString(),
        created: new Date().toISOString(),
        '1.0.1': new Date().toISOString()
      },
      'dist-tags': { latest: '1.0.1' },
      maintainers: [{ name: 'zkat', email: 'idk@idk.tech' }],
      versions: {
        '1.0.1': {
          _id: 'libnpmpublish@1.0.1',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          name: 'libnpmpublish',
          version: '1.0.1',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.1.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.1.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }))
    const newPackument = cloneDeep(Object.assign({}, basePackument, {
      'dist-tags': { latest: '1.0.0' },
      maintainers: [{ name: 'other', email: 'other@idk.tech' }],
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          _npmUser: {
            name: 'other',
            email: 'other@idk.tech'
          },
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          maintainers: [{ name: 'other', email: 'other@idk.tech' }],
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }))
    const mergedPackument = cloneDeep(Object.assign({}, basePackument, {
      time: currentPackument.time,
      'dist-tags': { latest: '1.0.0' },
      maintainers: currentPackument.maintainers,
      versions: Object.assign({}, currentPackument.versions, newPackument.versions),
      _attachments: Object.assign({}, currentPackument._attachments, newPackument._attachments)
    }))
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.notOk(body._rev, 'no _rev in initial post')
      t.deepEqual(body, newPackument, 'got conflicting packument')
      return true
    }).reply(409, { error: 'gimme _rev plz' })
    srv.get('/libnpmpublish?write=true').reply(200, Object.assign({
      _rev: REV
    }, currentPackument))
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, Object.assign({
        _rev: REV
      }, mergedPackument), 'posted packument includes _rev and a merged version')
      return true
    }).reply(201, {})
    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      username: 'other',
      email: 'other@idk.tech'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('retry after a conflict -- no versions on remote', t => {
  const REV = '72-47f2986bfd8e8b55068b204588bbf484'
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const basePackument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish'
    }
    const currentPackument = cloneDeep(Object.assign({}, basePackument, {
      maintainers: [{ name: 'zkat', email: 'idk@idk.tech' }]
    }))
    const newPackument = cloneDeep(Object.assign({}, basePackument, {
      'dist-tags': { latest: '1.0.0' },
      maintainers: [{ name: 'other', email: 'other@idk.tech' }],
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          _npmUser: {
            name: 'other',
            email: 'other@idk.tech'
          },
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          maintainers: [{ name: 'other', email: 'other@idk.tech' }],
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }))
    const mergedPackument = cloneDeep(Object.assign({}, basePackument, {
      'dist-tags': { latest: '1.0.0' },
      maintainers: currentPackument.maintainers,
      versions: Object.assign({}, currentPackument.versions, newPackument.versions),
      _attachments: Object.assign({}, currentPackument._attachments, newPackument._attachments)
    }))
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.notOk(body._rev, 'no _rev in initial post')
      t.deepEqual(body, newPackument, 'got conflicting packument')
      return true
    }).reply(409, { error: 'gimme _rev plz' })
    srv.get('/libnpmpublish?write=true').reply(200, Object.assign({
      _rev: REV
    }, currentPackument))
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, Object.assign({
        _rev: REV
      }, mergedPackument), 'posted packument includes _rev and a merged version')
      return true
    }).reply(201, {})
    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      username: 'other',
      email: 'other@idk.tech'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})
test('version conflict', t => {
  const REV = '72-47f2986bfd8e8b55068b204588bbf484'
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const basePackument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {},
      versions: {},
      _attachments: {}
    }
    const newPackument = cloneDeep(Object.assign({}, basePackument, {
      'dist-tags': { latest: '1.0.0' },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }))
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.notOk(body._rev, 'no _rev in initial post')
      t.deepEqual(body, newPackument, 'got conflicting packument')
      return true
    }).reply(409, { error: 'gimme _rev plz' })
    srv.get('/libnpmpublish?write=true').reply(200, Object.assign({
      _rev: REV
    }, newPackument))
    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      token: 'deadbeef'
    })).then(
      () => { throw new Error('should not succeed') },
      err => {
        t.equal(err.code, 'EPUBLISHCONFLICT', 'got publish conflict code')
      }
    )
  })
})

test('publish with basic auth', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      maintainers: [{
        name: 'zkat',
        email: 'kat@example.tech'
      }],
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          _npmUser: {
            name: 'zkat',
            email: 'kat@example.tech'
          },
          maintainers: [{
            name: 'zkat',
            email: 'kat@example.tech'
          }],
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: /^Basic /
    }).reply(201, {})

    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      username: 'zkat',
      email: 'kat@example.tech'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('publish base64 string', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    return publish(manifest, tarData.toString('base64'), OPTS.concat({
      npmVersion: '6.9.0',
      token: 'deadbeef'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('publish tar stream', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    const stream = new PassThrough()
    setTimeout(() => stream.end(tarData), 0)
    return publish(manifest, stream, OPTS.concat({
      npmVersion: '6.9.0',
      token: 'deadbeef'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('refuse if package marked private', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff',
    private: true
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      token: 'deadbeef'
    })).then(
      () => { throw new Error('should not have succeeded') },
      err => {
        t.equal(err.code, 'EPRIVATE', 'got correct error code')
      }
    )
  })
})

test('publish includes access', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      access: 'public',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    return publish(manifest, tarData, OPTS.concat({
      token: 'deadbeef',
      access: 'public'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('refuse if package is unscoped plus `restricted` access', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      access: 'restricted'
    })).then(
      () => { throw new Error('should not have succeeded') },
      err => {
        t.equal(err.code, 'EUNSCOPED', 'got correct error code')
      }
    )
  })
})

test('refuse if tarball is wrong type', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return publish(manifest, { data: 42 }, OPTS.concat({
    npmVersion: '6.9.0',
    token: 'deadbeef'
  })).then(
    () => { throw new Error('should not have succeeded') },
    err => {
      t.equal(err.code, 'EBADTAR', 'got correct error code')
    }
  )
})

test('refuse if bad semver on manifest', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: 'lmao',
    description: 'some stuff'
  }
  return publish(manifest, 'deadbeef', OPTS).then(
    () => { throw new Error('should not have succeeded') },
    err => {
      t.equal(err.code, 'EBADSEMVER', 'got correct error code')
    }
  )
})

test('other error code', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          _npmVersion: '6.9.0',
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(500, { error: 'go away' })

    return publish(manifest, tarData, OPTS.concat({
      npmVersion: '6.9.0',
      token: 'deadbeef'
    })).then(
      () => { throw new Error('should not succeed') },
      err => {
        t.match(err.message, /go away/, 'no retry on non-409')
      }
    )
  })
})

test('publish includes access', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff'
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      access: 'public',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    return publish(manifest, tarData, OPTS.concat({
      token: 'deadbeef',
      access: 'public'
    })).then(() => {
      t.ok(true, 'publish succeeded')
    })
  })
})

test('publishConfig on manifest', t => {
  const manifest = {
    name: 'libnpmpublish',
    version: '1.0.0',
    description: 'some stuff',
    publishConfig: {
      registry: REG
    }
  }
  return mockTar({
    'package.json': JSON.stringify(manifest),
    'index.js': 'console.log("hello world")'
  }).then(tarData => {
    const shasum = crypto.createHash('sha1').update(tarData).digest('hex')
    const integrity = ssri.fromData(tarData, { algorithms: ['sha512'] })
    const packument = {
      name: 'libnpmpublish',
      description: 'some stuff',
      readme: '',
      _id: 'libnpmpublish',
      'dist-tags': {
        latest: '1.0.0'
      },
      versions: {
        '1.0.0': {
          _id: 'libnpmpublish@1.0.0',
          _nodeVersion: process.versions.node,
          name: 'libnpmpublish',
          version: '1.0.0',
          description: 'some stuff',
          dist: {
            shasum,
            integrity: integrity.toString(),
            tarball: `http://mock.reg/libnpmpublish/-/libnpmpublish-1.0.0.tgz`
          },
          publishConfig: {
            registry: REG
          }
        }
      },
      _attachments: {
        'libnpmpublish-1.0.0.tgz': {
          'content_type': 'application/octet-stream',
          data: tarData.toString('base64'),
          length: tarData.length
        }
      }
    }
    const srv = tnock(t, REG)
    srv.put('/libnpmpublish', body => {
      t.deepEqual(body, packument, 'posted packument matches expectations')
      return true
    }, {
      authorization: 'Bearer deadbeef'
    }).reply(201, {})

    return publish(manifest, tarData, { token: 'deadbeef' }).then(ret => {
      t.ok(ret, 'publish succeeded')
    })
  })
})
