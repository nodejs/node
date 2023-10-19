const t = require('tap')
const fs = require('fs/promises')
const path = require('path')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

t.test('call with args', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      call: 'foo',
    },
  })

  await t.rejects(
    npm.exec('exec', ['bar']),
    { code: 'EUSAGE' }
  )
})

t.test('registry package', async t => {
  const registry = new MockRegistry({
    tap: t,
    registry: 'https://registry.npmjs.org/',
  })

  const manifest = registry.manifest({ name: '@npmcli/npx-test' })
  manifest.versions['1.0.0'].bin = { 'npx-test': 'index.js' }

  const { npm } = await loadMockNpm(t, {
    config: {
      audit: false,
      yes: true,
    },
    prefixDir: {
      'npm-exec-test': {
        'package.json': JSON.stringify(manifest),
        'index.js': `#!/usr/bin/env node
  require('fs').writeFileSync('npm-exec-test-success', '')`,
      },
    },
  })

  await registry.package({
    manifest,
    tarballs: {
      '1.0.0': path.join(npm.prefix, 'npm-exec-test'),
    } })

  await npm.exec('exec', ['@npmcli/npx-test'])
  const exists = await fs.stat(path.join(npm.prefix, 'npm-exec-test-success'))
  t.ok(exists.isFile(), 'bin ran, creating file')
})

t.test('--prefix', async t => {
  const registry = new MockRegistry({
    tap: t,
    registry: 'https://registry.npmjs.org/',
  })

  const manifest = registry.manifest({ name: '@npmcli/npx-test' })
  manifest.versions['1.0.0'].bin = { 'npx-test': 'index.js' }

  const { npm } = await loadMockNpm(t, {
    config: {
      audit: false,
      yes: true,
    },
    prefixDir: {
      'npm-exec-test': {
        'package.json': JSON.stringify(manifest),
        'index.js': `#!/usr/bin/env node
  require('fs').writeFileSync('npm-exec-test-success', '')`,
      },
    },
  })

  // This is what `--prefix` does
  npm.globalPrefix = npm.localPrefix

  await registry.package({
    manifest,
    tarballs: {
      '1.0.0': path.join(npm.prefix, 'npm-exec-test'),
    } })

  await npm.exec('exec', ['@npmcli/npx-test'])
  const exists = await fs.stat(path.join(npm.prefix, 'npm-exec-test-success'))
  t.ok(exists.isFile(), 'bin ran, creating file')
})

t.test('workspaces', async t => {
  const registry = new MockRegistry({
    tap: t,
    registry: 'https://registry.npmjs.org/',
  })

  const manifest = registry.manifest({ name: '@npmcli/npx-test' })
  manifest.versions['1.0.0'].bin = { 'npx-test': 'index.js' }

  const { npm } = await loadMockNpm(t, {
    config: {
      audit: false,
      yes: true,
      workspace: ['workspace-a'],
    },
    prefixDir: {
      'npm-exec-test': {
        'package.json': JSON.stringify(manifest),
        'index.js': `#!/usr/bin/env node
  require('fs').writeFileSync('npm-exec-test-success', '')`,
      },
      'package.json': JSON.stringify({
        name: '@npmcli/npx-workspace-test',
        workspaces: ['workspace-a'],
      }),
      'workspace-a': {
        'package.json': JSON.stringify({
          name: 'workspace-a',
        }),
      },
    },
  })

  await registry.package({ manifest,
    tarballs: {
      '1.0.0': path.join(npm.prefix, 'npm-exec-test'),
    } })
  await npm.exec('exec', ['@npmcli/npx-test'])
  const exists = await fs.stat(path.join(npm.prefix, 'workspace-a', 'npm-exec-test-success'))
  t.ok(exists.isFile(), 'bin ran, creating file inside workspace')
})

t.test('npx --no-install @npmcli/npx-test', async t => {
  const registry = new MockRegistry({
    tap: t,
    registry: 'https://registry.npmjs.org/',
  })

  const manifest = registry.manifest({ name: '@npmcli/npx-test' })
  manifest.versions['1.0.0'].bin = { 'npx-test': 'index.js' }

  const { npm } = await loadMockNpm(t, {
    config: {
      audit: false,
      yes: false,
    },
    prefixDir: {
      'npm-exec-test': {
        'package.json': JSON.stringify(manifest),
        'index.js': `#!/usr/bin/env node
  require('fs').writeFileSync('npm-exec-test-success', '')`,
      },
    },
  })

  try {
    await npm.exec('exec', ['@npmcli/npx-test'])
    t.fail('Expected error was not thrown')
  } catch (error) {
    t.match(
      error.message,
      'npx canceled due to missing packages and no YES option: ',
      'Expected error message thrown'
    )
  }
})
