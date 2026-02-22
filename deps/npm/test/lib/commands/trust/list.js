const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const packageName = '@npmcli/test-package'

t.test('list with package name argument', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  const trustConfigs = [
    {
      id: 'test-id-1',
      type: 'github',
      claims: {
        repository: 'owner/repo',
        workflow_ref: {
          file: 'test.yml',
        },
      },
      environment: 'production',
    },
    {
      id: 'test-id-2',
      type: 'gitlab',
      claims: {
        project_id: '12345',
        ref_path: 'refs/heads/main',
        pipeline_source: 'push',
      },
    },
  ]

  registry.trustList({ packageName, body: trustConfigs })

  await npm.exec('trust', ['list', packageName])
})

t.test('list without package name (uses package.json)', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  const trustConfigs = [
    {
      id: 'test-id-1',
      type: 'github',
      claims: {
        repository: 'owner/repo',
        workflow_ref: {
          file: 'workflow.yml',
        },
      },
    },
  ]

  registry.trustList({ packageName, body: trustConfigs })

  await npm.exec('trust', ['list'])
})

t.test('list with no trust configurations', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustList({ packageName, body: [] })

  await npm.exec('trust', ['list', packageName])
})

t.test('list without package name and no package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {},
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['list']),
    { message: /Package name must be specified either as an argument or in the package\.json file/ }
  )
})

t.test('list without package name and no name in package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['list']),
    { message: /Package name must be specified either as an argument or in the package\.json file/ }
  )
})

t.test('list with --json flag', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      json: true,
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  const trustConfigs = [
    {
      id: 'test-id-1',
      type: 'github',
      claims: {
        repository: 'owner/repo',
        workflow_ref: {
          file: 'test.yml',
        },
      },
      environment: 'production',
    },
  ]

  registry.trustList({ packageName, body: trustConfigs })

  await npm.exec('trust', ['list', packageName])
})

t.test('list with scoped package', async t => {
  const scopedPackage = '@scope/package'
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: scopedPackage,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  const trustConfigs = [
    {
      id: 'test-id-1',
      type: 'github',
      claims: {
        repository: 'owner/repo',
        workflow_ref: {
          file: 'test.yml',
        },
      },
    },
  ]

  registry.trustList({ packageName: scopedPackage, body: trustConfigs })

  await npm.exec('trust', ['list', scopedPackage])
})

t.test('list with unknown trust type', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  const trustConfigs = [
    {
      id: 'test-id-1',
      type: 'unknown-type',
      claims: {
        custom: 'value',
      },
    },
  ]

  registry.trustList({ packageName, body: trustConfigs })

  await npm.exec('trust', ['list', packageName])
})
