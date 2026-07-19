const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const packageName = '@npmcli/test-package'

t.test('revoke with package name argument and id', async t => {
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

  const trustId = 'test-id-1'
  registry.trustRevoke({ packageName, id: trustId })

  await npm.exec('trust', ['revoke', packageName, '--id', trustId])
})

t.test('revoke without package name (uses package.json)', async t => {
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

  const trustId = 'test-id-2'
  registry.trustRevoke({ packageName, id: trustId })

  await npm.exec('trust', ['revoke', '--id', trustId])
})

t.test('revoke with dry-run flag', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      'dry-run': true,
    },
  })

  // No registry mock needed since dry-run should not make network requests
  const trustId = 'test-id-3'

  await npm.exec('trust', ['revoke', packageName, '--id', trustId])
})

t.test('revoke without package name and no package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {},
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['revoke', '--id', 'test-id']),
    { message: /Package name must be specified either as an argument or in the package\.json file/ }
  )
})

t.test('revoke without package name and no name in package.json', async t => {
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
    npm.exec('trust', ['revoke', '--id', 'test-id']),
    { message: /Package name must be specified either as an argument or in the package\.json file/ }
  )
})

t.test('revoke without id flag', async t => {
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

  await t.rejects(
    npm.exec('trust', ['revoke', packageName]),
    { message: /ID of the trusted relationship to revoke must be specified with the --id option/ }
  )
})

t.test('revoke with scoped package', async t => {
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

  const trustId = 'test-id-scoped'
  registry.trustRevoke({ packageName: scopedPackage, id: trustId })

  await npm.exec('trust', ['revoke', scopedPackage, '--id', trustId])
})

t.test('revoke with special characters in id', async t => {
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

  const trustId = 'test-id/with:special@chars'
  registry.trustRevoke({ packageName, id: trustId })

  await npm.exec('trust', ['revoke', packageName, '--id', trustId])
})

t.test('revoke with 404 response', async t => {
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

  const trustId = 'non-existent-id'
  registry.trustRevoke({
    packageName,
    id: trustId,
    responseCode: 404,
    body: { error: 'Not Found' },
  })

  await t.rejects(
    npm.exec('trust', ['revoke', packageName, '--id', trustId]),
    { statusCode: 404 }
  )
})

t.test('revoke with 500 response', async t => {
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

  const trustId = 'test-id-error'
  registry.trustRevoke({
    packageName,
    id: trustId,
    responseCode: 500,
    body: { error: 'Internal Server Error' },
  })

  await t.rejects(
    npm.exec('trust', ['revoke', packageName, '--id', trustId]),
    { statusCode: 500 }
  )
})

t.test('revoke with unscoped package name', async t => {
  const unscopedPackage = 'test-package'
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: unscopedPackage,
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

  const trustId = 'test-id-unscoped'
  registry.trustRevoke({ packageName: unscopedPackage, id: trustId })

  await npm.exec('trust', ['revoke', unscopedPackage, '--id', trustId])
})

t.test('revoke with very long id', async t => {
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

  const trustId = 'a'.repeat(100)
  registry.trustRevoke({ packageName, id: trustId })

  await npm.exec('trust', ['revoke', packageName, '--id', trustId])
})

t.test('revoke with UUID id format', async t => {
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

  const trustId = '550e8400-e29b-41d4-a716-446655440000'
  registry.trustRevoke({ packageName, id: trustId })

  await npm.exec('trust', ['revoke', packageName, '--id', trustId])
})
