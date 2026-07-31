const t = require('tap')
const fs = require('node:fs')
const { resolve } = require('node:path')
const setupMockNpm = require('../../fixtures/mock-npm')

t.test('no args', async t => {
  const { npm, joinedOutput, prefix: path } = await setupMockNpm(t, {
    prefixDir: {
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

  await npm.exec('rebuild', [])

  t.ok(() => fs.statSync(aBuildFile))
  t.ok(() => fs.statSync(bBuildFile))
  t.ok(() => fs.statSync(aBinFile))
  t.ok(() => fs.statSync(bBinFile))

  t.equal(
    joinedOutput(),
    'rebuilt dependencies successfully',
    'should output success msg'
  )
})

t.test('filter by pkg name', async t => {
  const { npm, prefix: path } = await setupMockNpm(t, {
    prefixDir: {
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
    },
  })

  const aBinFile = resolve(path, 'node_modules/.bin/a')
  const bBinFile = resolve(path, 'node_modules/.bin/b')
  t.throws(() => fs.statSync(aBinFile))
  t.throws(() => fs.statSync(bBinFile))

  await npm.exec('rebuild', ['b'])

  t.throws(() => fs.statSync(aBinFile), 'should not link a bin')
  t.ok(() => fs.statSync(bBinFile), 'should link filtered pkg bin')
})

t.test('filter by pkg@<range>', async t => {
  const { npm, prefix: path } = await setupMockNpm(t, {
    prefixDir: {
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
    },
  })

  const bBinFile = resolve(path, 'node_modules/.bin/b')
  const nestedBinFile = resolve(path, 'node_modules/a/node_modules/.bin/b')

  await npm.exec('rebuild', ['b@2'])

  t.throws(() => fs.statSync(bBinFile), 'should not link b bin')
  t.ok(() => fs.statSync(nestedBinFile), 'should link filtered pkg bin')
})

t.test('filter by directory', async t => {
  const { npm, prefix: path } = await setupMockNpm(t, {
    prefixDir: {
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
    },
  })

  const aBinFile = resolve(path, 'node_modules/.bin/a')
  const bBinFile = resolve(path, 'node_modules/.bin/b')
  t.throws(() => fs.statSync(aBinFile))
  t.throws(() => fs.statSync(bBinFile))

  await npm.exec('rebuild', ['file:node_modules/b'])

  t.throws(() => fs.statSync(aBinFile), 'should not link a bin')
  t.ok(() => fs.statSync(bBinFile), 'should link filtered pkg bin')
})

t.test('filter must be a semver version/range, or directory', async t => {
  const { npm } = await setupMockNpm(t)

  await t.rejects(
    npm.exec('rebuild', ['git+ssh://github.com/npm/arborist']),
    /`npm rebuild` only supports SemVer version\/range specifiers/,
    'should throw type error'
  )
})

t.test('global prefix', async t => {
  const { npm, globalPrefix, joinedOutput } = await setupMockNpm(t, {
    config: {
      global: true,
    },
    globalPrefixDir: {
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

  await npm.exec('rebuild', [])
  t.ok(() => fs.statSync(resolve(globalPrefix, 'lib/node_modules/.bin/a')))

  t.equal(
    joinedOutput(),
    'rebuilt dependencies successfully',
    'should output success msg'
  )
})

t.test('completion', async t => {
  const { rebuild } = await setupMockNpm(t, {
    command: 'rebuild',
    prefixDir: {
      node_modules: {
        foo: {
          'package.json': JSON.stringify({ name: 'foo', version: '1.0.0' }),
        },
      },
      'package.json': JSON.stringify({ name: 'project', version: '1.0.0' }),
    },
  })
  const res = await rebuild.completion({ conf: { argv: { remain: ['npm', 'rebuild'] } } })
  t.type(res, Array)
})

t.test('emits blocked warning for unreviewed install scripts', async t => {
  const { npm, logs } = await setupMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'host', version: '1.0.0' }),
      node_modules: {
        canvas: {
          'package.json': JSON.stringify({
            name: 'canvas',
            version: '1.0.0',
            scripts: { install: 'echo install' },
          }),
        },
      },
    },
  })
  await npm.exec('rebuild', [])
  t.match(
    logs.warn.byTitle('rebuild'),
    [/install scripts blocked because they are not covered by allowScripts/]
  )
})

t.test('global advisory warning points at npm config set, not approve-scripts', async t => {
  const { npm, logs } = await setupMockNpm(t, {
    config: {
      global: true,
    },
    globalPrefixDir: {
      node_modules: {
        canvas: {
          'index.js': '',
          'package.json': JSON.stringify({
            name: 'canvas',
            version: '1.0.0',
            scripts: { install: 'echo install' },
          }),
        },
      },
    },
  })
  await npm.exec('rebuild', [])
  const warn = logs.warn.byTitle('rebuild').join('\n')
  t.match(warn, /install scripts blocked because they are not covered by allowScripts/)
  t.match(warn, /npm config set allow-scripts=canvas/)
  t.notMatch(warn, /approve-scripts/)
})

t.test('no advisory warning when allowScripts covers the package', async t => {
  const { npm, logs } = await setupMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { canvas: '1.0.0' },
        allowScripts: { canvas: true },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { canvas: '1.0.0' } },
          'node_modules/canvas': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/canvas/-/canvas-1.0.0.tgz',
            hasInstallScript: true,
          },
        },
      }),
      node_modules: {
        canvas: {
          'package.json': JSON.stringify({
            name: 'canvas',
            version: '1.0.0',
            scripts: { install: 'echo install' },
          }),
        },
      },
    },
  })
  await npm.exec('rebuild', [])
  t.strictSame(logs.warn.byTitle('rebuild'), [])
})

t.test('rebuild <pkg> honors the gate for an unreviewed package', async t => {
  const { npm, logs, prefix: path } = await setupMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { canvas: '1.0.0' },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { canvas: '1.0.0' } },
          'node_modules/canvas': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/canvas/-/canvas-1.0.0.tgz',
            hasInstallScript: true,
          },
        },
      }),
      node_modules: {
        canvas: {
          'package.json': JSON.stringify({
            name: 'canvas',
            version: '1.0.0',
            scripts: {
              install: "node -e \"require('fs').writeFileSync('ran', '')\"",
            },
          }),
        },
      },
    },
  })

  const ranFile = resolve(path, 'node_modules/canvas/ran')
  t.throws(() => fs.statSync(ranFile))

  await npm.exec('rebuild', ['canvas'])

  t.throws(() => fs.statSync(ranFile), 'unreviewed install script must not run')
  t.match(
    logs.warn.byTitle('rebuild'),
    [/install scripts blocked because they are not covered by allowScripts/]
  )
})

t.test('rebuild <name> never targets a bundled dependency', async t => {
  const { npm, prefix: path } = await setupMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { parent: '1.0.0' },
      }),
      node_modules: {
        parent: {
          'index.js': '',
          'package.json': JSON.stringify({
            name: 'parent',
            version: '1.0.0',
            bundleDependencies: ['bcrypt'],
            dependencies: { bcrypt: '1.0.0' },
          }),
          node_modules: {
            bcrypt: {
              'index.js': '',
              'package.json': JSON.stringify({
                name: 'bcrypt',
                version: '1.0.0',
                bin: 'index.js',
                scripts: {
                  install: "node -e \"require('fs').writeFileSync('ran', '')\"",
                },
              }),
            },
          },
        },
      },
    },
  })

  const ranFile = resolve(path, 'node_modules/parent/node_modules/bcrypt/ran')
  t.throws(() => fs.statSync(ranFile))

  await npm.exec('rebuild', ['bcrypt'])

  t.throws(
    () => fs.statSync(ranFile),
    'bundled bcrypt install script must not run'
  )
})
