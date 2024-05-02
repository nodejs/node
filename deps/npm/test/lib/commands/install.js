const t = require('tap')
const tspawk = require('../../fixtures/tspawk')
const MockRegistry = require('@npmcli/mock-registry')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const path = require('node:path')

const spawk = tspawk(t)

const abbrev = {
  'package.json': '{"name": "abbrev", "version": "1.0.0"}',
  test: 'test file',
}

const packageJson = {
  name: '@npmcli/test-package',
  version: '1.0.0',
  dependencies: {
    abbrev: '^1.0.0',
  },
}

// tspawk calls preventUnmatched which assures that no scripts run if we don't mock any

t.test('exec commands', async t => {
  await t.test('with args does not run lifecycle scripts', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        audit: false,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          ...packageJson,
          scripts: {
            install: 'echo install',
          },
        }),
        abbrev,
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })

    const manifest = registry.manifest({ name: 'abbrev' })
    await registry.package({ manifest })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.prefix, 'abbrev'),
    })

    await npm.exec('install', ['abbrev'])
  })

  await t.test('without args runs lifecycle scripts', async t => {
    const lifecycleScripts = [
      'preinstall',
      'install',
      'postinstall',
      'prepublish',
      'preprepare',
      'prepare',
      'postprepare',
    ]
    const scripts = {}
    for (const script of lifecycleScripts) {
      spawk.spawn(/.*/, a => {
        runOrder.push(script)
        return a.includes(`${script} lifecycle script`)
      })
      scripts[script] = `${script} lifecycle script`
    }
    const { npm } = await loadMockNpm(t, {
      config: {
        audit: false,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          ...packageJson,
          scripts,
        }),
        abbrev,
      },
    })
    const runOrder = []
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    const manifest = registry.manifest({ name: 'abbrev' })
    await registry.package({ manifest })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.prefix, 'abbrev'),
    })

    await npm.exec('install')
    t.strictSame(lifecycleScripts, runOrder, 'all script ran in the correct order')
  })

  await t.test('should ignore scripts with --ignore-scripts', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        'ignore-scripts': true,
        audit: false,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          ...packageJson,
          scripts: {
            install: 'echo install',
          },
        }),
        abbrev,
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })

    const manifest = registry.manifest({ name: 'abbrev' })
    await registry.package({ manifest })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.prefix, 'abbrev'),
    })

    await npm.exec('install')
  })

  await t.test('should not install invalid global package name', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        global: true,
      },
    })
    await t.rejects(
      npm.exec('install', ['']),
      /Usage:/,
      'should not install invalid package name'
    )
  })

  await t.test('npm i -g npm engines check success', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        npm: {
          'package.json': JSON.stringify({ name: 'npm', version: '1.0.0' }),
          'index.js': 'console.log("this is npm")',
        },
      },
      config: { global: true },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    const manifest = registry.manifest({
      name: 'npm',
      packuments: [{ version: '1.0.0', engines: { node: '>1' } }],
    })
    await registry.package({ manifest, times: 2 })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.localPrefix, 'npm'),
    })
    await npm.exec('install', ['npm'])
    t.ok('No exceptions happen')
  })

  await t.test('npm i -g npm engines check failure', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        npm: {
          'package.json': JSON.stringify({ name: 'npm', version: '1.0.0' }),
          'index.js': 'console.log("this is the npm we are installing")',
        },
      },
      config: { global: true },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    const manifest = registry.manifest({
      name: 'npm',
      packuments: [{ version: '1.0.0', engines: { node: '~1' } }],
    })
    await registry.package({ manifest })
    await t.rejects(
      npm.exec('install', ['npm']),
      {
        message: 'Unsupported engine',
        pkgid: 'npm@1.0.0',
        current: {
          node: process.version,
          npm: '1.0.0',
        },
        required: {
          node: '~1',
        },
        code: 'EBADENGINE',
      }
    )
  })

  await t.test('npm i -g npm engines check failure forced override', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        npm: {
          'package.json': JSON.stringify({ name: 'npm', version: '1.0.0' }),
          'index.js': 'console.log("this is npm")',
        },
      },
      config: { global: true, force: true },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    const manifest = registry.manifest({
      name: 'npm',
      packuments: [{ version: '1.0.0', engines: { node: '~1' } }],
    })
    await registry.package({ manifest, times: 2 })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.localPrefix, 'npm'),
    })
    await npm.exec('install', ['npm'])
    t.ok('No exceptions happen')
  })
})

t.test('completion', async t => {
  const mockComp = async (t, { noChdir } = {}) => loadMockNpm(t, {
    command: 'install',
    prefixDir: {
      arborist: {
        'package.json': '{}',
      },
      'arborist.txt': 'just a file',
      'other-dir': { a: 'a' },
    },
    ...(noChdir ? { chdir: false } : {}),
  })

  await t.test('completion to folder - has a match', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './ar' })
    t.strictSame(res, ['arborist'], 'package dir match')
  })

  await t.test('completion to folder - invalid dir', async t => {
    const { install } = await mockComp(t, { noChdir: true })
    const res = await install.completion({ partialWord: '/does/not/exist' })
    t.strictSame(res, [], 'invalid dir: no matching')
  })

  await t.test('completion to folder - no matches', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './pa' })
    t.strictSame(res, [], 'no name match')
  })

  await t.test('completion to folder - match is not a package', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './othe' })
    t.strictSame(res, [], 'no name match')
  })

  await t.test('completion to url', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: 'http://path/to/url' })
    t.strictSame(res, [])
  })

  await t.test('no /', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: 'toto' })
    t.notOk(res)
  })

  await t.test('only /', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: '/' })
    t.strictSame(res, [])
  })
})
