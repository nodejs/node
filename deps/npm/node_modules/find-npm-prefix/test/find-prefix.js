'use strict'
const Bluebird = require('bluebird')
const test = require('tap').test
const requireInject = require('require-inject')
const findPrefix = requireInject('../find-prefix.js', {
  fs: {
    readdir: mockReaddir
  }
})

test('find-prefix', t => {
  const tests = {
    '/Users/example/code/test1/node_modules': '/Users/example/code/test1',
    '/Users/example/code/test1/node_modules/node_modules': '/Users/example/code/test1',
    '/Users/example/code/test1/sub1': '/Users/example/code/test1',
    '/Users/example/code/test1/sub1/sub1a': '/Users/example/code/test1',
    '/Users/example/code/test2': '/Users/example/code/test2',
    '/Users/example/code/test2/sub2': '/Users/example/code/test2',
    '/Users/example/code': '/Users/example/code',
    '/Users/example': '/Users/example',
    '/does/not/exist': '/does/not/exist'
  }
  t.plan(Object.keys(tests).length)
  return Bluebird.map(Object.keys(tests), dir => {
    return findPrefix(dir).then(pre => {
      t.is(pre, tests[dir], dir)
    })
  })
})

test('fail-prefix', t => {
  return findPrefix('/Users/example/eperm').then(pre => {
    t.fail('no eperm')
  }).catch(err => {
    t.is(err.code, 'EPERM', 'got perm error')
  })
})

const fixture = {
  'Users': {
    'example': {
      'code': {
        'test1': {
          'node_modules': {
            'node_modules': {}
          },
          'sub1': {
            'sub1a': {}
          }
        },
        'test2': {
          'package.json': {},
          'sub2': {}
        }
      }
    }
  }
}

function mockReaddir (dir, cb) {
  if (/eperm/.test(dir)) {
    const err = new Error('Can not read: ' + dir)
    err.code = 'EPERM'
    return cb(err)
  }
  const parts = dir.split(/\//).slice(1)
  let cwd = fixture
  let part
  while ((part = parts.shift())) {
    if (part in cwd) {
      cwd = cwd[part]
    } else {
      const err = new Error('Does not exist: ' + dir + ' * ' + part)
      err.code = 'ENOENT'
      return cb(err)
    }
  }
  return cb(null, Object.keys(cwd))
}
