const t = require('tap')
const fs = require('fs')
const { resolve } = require('path')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

let result = ''

const config = {
  global: false,
}
const npm = mockNpm({
  globalDir: '',
  config,
  prefix: '',
  output: (...msg) => {
    result += msg.join('\n')
  },
})
const Rebuild = require('../../../lib/commands/rebuild.js')
const rebuild = new Rebuild(npm)

t.afterEach(() => {
  npm.prefix = ''
  config.global = false
  npm.globalDir = ''
  result = ''
})

t.test('no args', async t => {
  const path = t.testdir({
    node_modules: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          bin: 'cwd',
          scripts: {
            preinstall: "node -e \"require('fs').writeFileSync('cwd', '')\"",
          },
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          bin: 'cwd',
          scripts: {
            preinstall: "node -e \"require('fs').writeFileSync('cwd', '')\"",
          },
        }),
      },
    },
  })

  const aBuildFile = resolve(path, 'node_modules/a/cwd')
  const bBuildFile = resolve(path, 'node_modules/b/cwd')
  const aBinFile = resolve(path, 'node_modules/.bin/a')
  const bBinFile = resolve(path, 'node_modules/.bin/b')
  t.throws(() => fs.statSync(aBuildFile))
  t.throws(() => fs.statSync(bBuildFile))
  t.throws(() => fs.statSync(aBinFile))
  t.throws(() => fs.statSync(bBinFile))

  npm.prefix = path

  await rebuild.exec([])

  t.ok(() => fs.statSync(aBuildFile))
  t.ok(() => fs.statSync(bBuildFile))
  t.ok(() => fs.statSync(aBinFile))
  t.ok(() => fs.statSync(bBinFile))

  t.equal(
    result,
    'rebuilt dependencies successfully',
    'should output success msg'
  )
})

t.test('filter by pkg name', async t => {
  const path = t.testdir({
    node_modules: {
      a: {
        'index.js': '',
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          bin: 'index.js',
        }),
      },
      b: {
        'index.js': '',
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          bin: 'index.js',
        }),
      },
    },
  })

  npm.prefix = path

  const aBinFile = resolve(path, 'node_modules/.bin/a')
  const bBinFile = resolve(path, 'node_modules/.bin/b')
  t.throws(() => fs.statSync(aBinFile))
  t.throws(() => fs.statSync(bBinFile))

  await rebuild.exec(['b'])

  t.throws(() => fs.statSync(aBinFile), 'should not link a bin')
  t.ok(() => fs.statSync(bBinFile), 'should link filtered pkg bin')
})

t.test('filter by pkg@<range>', async t => {
  const path = t.testdir({
    node_modules: {
      a: {
        'index.js': '',
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          bin: 'index.js',
        }),
        node_modules: {
          b: {
            'index.js': '',
            'package.json': JSON.stringify({
              name: 'b',
              version: '2.0.0',
              bin: 'index.js',
            }),
          },
        },
      },
      b: {
        'index.js': '',
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          bin: 'index.js',
        }),
      },
    },
  })

  npm.prefix = path

  const bBinFile = resolve(path, 'node_modules/.bin/b')
  const nestedBinFile = resolve(path, 'node_modules/a/node_modules/.bin/b')

  await rebuild.exec(['b@2'])

  t.throws(() => fs.statSync(bBinFile), 'should not link b bin')
  t.ok(() => fs.statSync(nestedBinFile), 'should link filtered pkg bin')
})

t.test('filter by directory', async t => {
  const path = t.testdir({
    node_modules: {
      a: {
        'index.js': '',
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          bin: 'index.js',
        }),
      },
      b: {
        'index.js': '',
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          bin: 'index.js',
        }),
      },
    },
  })

  npm.prefix = path

  const aBinFile = resolve(path, 'node_modules/.bin/a')
  const bBinFile = resolve(path, 'node_modules/.bin/b')
  t.throws(() => fs.statSync(aBinFile))
  t.throws(() => fs.statSync(bBinFile))

  await rebuild.exec(['file:node_modules/b'])

  t.throws(() => fs.statSync(aBinFile), 'should not link a bin')
  t.ok(() => fs.statSync(bBinFile), 'should link filtered pkg bin')
})

t.test('filter must be a semver version/range, or directory', async t => {
  await t.rejects(
    rebuild.exec(['git+ssh://github.com/npm/arborist']),
    /`npm rebuild` only supports SemVer version\/range specifiers/,
    'should throw type error'
  )
})

t.test('global prefix', async t => {
  const globalPath = t.testdir({
    lib: {
      node_modules: {
        a: {
          'index.js': '',
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
            bin: 'index.js',
          }),
        },
      },
    },
  })

  config.global = true
  npm.globalDir = resolve(globalPath, 'lib', 'node_modules')

  await rebuild.exec([])
  t.ok(() => fs.statSync(resolve(globalPath, 'lib/node_modules/.bin/a')))

  t.equal(
    result,
    'rebuilt dependencies successfully',
    'should output success msg'
  )
})
