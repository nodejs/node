const t = require('tap')
const fs = require('fs')
const { resolve } = require('path')
const { fake: mockNpm } = require('../fixtures/mock-npm')

const npm = mockNpm({
  globalDir: '',
  config: {
    global: false,
    prefix: '',
  },
  localPrefix: '',
})
const mocks = {
  '../../lib/utils/reify-finish.js': () => Promise.resolve(),
}

const Uninstall = t.mock('../../lib/uninstall.js', mocks)
const uninstall = new Uninstall(npm)

t.afterEach(() => {
  npm.globalDir = ''
  npm.prefix = ''
  npm.localPrefix = ''
  npm.flatOptions.global = false
  npm.flatOptions.prefix = ''
})

t.test('remove single installed lib', t => {
  const path = t.testdir({
    'package.json': JSON.stringify({
      name: 'test-rm-single-lib',
      version: '1.0.0',
      dependencies: {
        a: '*',
        b: '*',
      },
    }),
    node_modules: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
        }),
      },
    },
    'package-lock.json': JSON.stringify({
      name: 'test-rm-single-lib',
      version: '1.0.0',
      lockfileVersion: 2,
      requires: true,
      packages: {
        '': {
          name: 'test-rm-single-lib',
          version: '1.0.0',
          dependencies: {
            a: '*',
          },
        },
        'node_modules/a': {
          version: '1.0.0',
        },
        'node_modules/b': {
          version: '1.0.0',
        },
      },
      dependencies: {
        a: {
          version: '1.0.0',
        },
        b: {
          version: '1.0.0',
        },
      },
    }),
  })

  const b = resolve(path, 'node_modules/b')
  t.ok(() => fs.statSync(b))

  npm.localPrefix = path

  uninstall.exec(['b'], err => {
    if (err)
      throw err

    t.throws(() => fs.statSync(b), 'should have removed package from npm')
    t.end()
  })
})

t.test('remove multiple installed libs', t => {
  const path = t.testdir({
    node_modules: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
        }),
      },
    },
    'package-lock.json': JSON.stringify({
      name: 'test-rm-single-lib',
      version: '1.0.0',
      lockfileVersion: 2,
      requires: true,
      packages: {
        '': {
          name: 'test-rm-single-lib',
          version: '1.0.0',
          dependencies: {
            a: '*',
          },
        },
        'node_modules/a': {
          version: '1.0.0',
        },
        'node_modules/b': {
          version: '1.0.0',
        },
      },
      dependencies: {
        a: {
          version: '1.0.0',
        },
        b: {
          version: '1.0.0',
        },
      },
    }),
  })

  const a = resolve(path, 'node_modules/a')
  const b = resolve(path, 'node_modules/b')
  t.ok(() => fs.statSync(a))
  t.ok(() => fs.statSync(b))

  npm.localPrefix = path

  uninstall.exec(['b'], err => {
    if (err)
      throw err

    t.throws(() => fs.statSync(a), 'should have removed a package from nm')
    t.throws(() => fs.statSync(b), 'should have removed b package from nm')
    t.end()
  })
})

t.test('no args local', t => {
  const path = t.testdir()

  npm.flatOptions.prefix = path

  uninstall.exec([], err => {
    t.match(
      err,
      /Must provide a package name to remove/,
      'should throw package name required error'
    )

    t.end()
  })
})

t.test('no args global', t => {
  const path = t.testdir({
    lib: {
      node_modules: {
        a: t.fixture('symlink', '../../projects/a'),
      },
    },
    projects: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
        }),
      },
    },
  })

  npm.localPrefix = resolve(path, 'projects', 'a')
  npm.globalDir = resolve(path, 'lib', 'node_modules')
  npm.config.set('global', true)

  const a = resolve(path, 'lib/node_modules/a')
  t.ok(() => fs.statSync(a))

  uninstall.exec([], err => {
    if (err)
      throw err

    t.throws(() => fs.statSync(a), 'should have removed global nm symlink')

    t.end()
  })
})

t.test('no args global but no package.json', t => {
  const path = t.testdir({})

  npm.prefix = path
  npm.localPrefix = path
  npm.flatOptions.global = true

  uninstall.exec([], err => {
    t.match(
      err,
      'npm uninstall'
    )

    t.end()
  })
})

t.test('unknown error reading from localPrefix package.json', t => {
  const path = t.testdir({})

  const Uninstall = t.mock('../../lib/uninstall.js', {
    ...mocks,
    'read-package-json-fast': () => Promise.reject(new Error('ERR')),
  })
  const uninstall = new Uninstall(npm)

  npm.prefix = path
  npm.localPrefix = path
  npm.flatOptions.global = true

  uninstall.exec([], err => {
    t.match(
      err,
      /ERR/,
      'should throw unknown error'
    )

    t.end()
  })
})
