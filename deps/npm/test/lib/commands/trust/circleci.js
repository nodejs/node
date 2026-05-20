const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')
const realProcLog = require('proc-log')

const packageName = '@npmcli/test-package'

t.test('circleci with all options provided', async t => {
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
    mocks: {
      'proc-log': {
        ...realProcLog,
        input: {
          ...realProcLog.input,
          read: async () => 'y',
        },
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({ packageName })

  await npm.exec('trust', [
    'circleci',
    packageName,
    '--yes',
    '--org-id', '550e8400-e29b-41d4-a716-446655440000',
    '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
    '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    '--vcs-origin', 'github.com/owner/repo',
    '--context-id', '123e4567-e89b-12d3-a456-426614174000',
  ])
})

t.test('circleci without optional context-id', async t => {
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
    mocks: {
      'proc-log': {
        ...realProcLog,
        input: {
          ...realProcLog.input,
          read: async () => 'y',
        },
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({ packageName })

  await npm.exec('trust', [
    'circleci',
    packageName,
    '--yes',
    '--org-id', '550e8400-e29b-41d4-a716-446655440000',
    '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
    '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    '--vcs-origin', 'github.com/owner/repo',
  ])
})

t.test('circleci with multiple context-ids', async t => {
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
    mocks: {
      'proc-log': {
        ...realProcLog,
        input: {
          ...realProcLog.input,
          read: async () => 'y',
        },
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({ packageName })

  await npm.exec('trust', [
    'circleci',
    packageName,
    '--yes',
    '--org-id', '550e8400-e29b-41d4-a716-446655440000',
    '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
    '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    '--vcs-origin', 'github.com/owner/repo',
    '--context-id', '123e4567-e89b-12d3-a456-426614174000',
    '--context-id', 'a1b2c3d4-e5f6-7890-abcd-ef1234567890',
  ])
})

t.test('circleci missing required org-id', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      '--vcs-origin', 'github.com/owner/repo',
    ]),
    { message: /org-id is required/ }
  )
})

t.test('circleci missing required project-id', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--org-id', '550e8400-e29b-41d4-a716-446655440000',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      '--vcs-origin', 'github.com/owner/repo',
    ]),
    { message: /project-id is required/ }
  )
})

t.test('circleci missing required pipeline-definition-id', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--org-id', '550e8400-e29b-41d4-a716-446655440000',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--vcs-origin', 'github.com/owner/repo',
    ]),
    { message: /pipeline-definition-id is required/ }
  )
})

t.test('circleci missing required vcs-origin', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--org-id', '550e8400-e29b-41d4-a716-446655440000',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    ]),
    { message: /vcs-origin is required/ }
  )
})

t.test('circleci with invalid org-id uuid format', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--org-id', 'not-a-uuid',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      '--vcs-origin', 'github.com/owner/repo',
    ]),
    { message: /org-id must be a valid UUID/ }
  )
})

t.test('circleci with invalid vcs-origin format', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--org-id', '550e8400-e29b-41d4-a716-446655440000',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      '--vcs-origin', 'invalid-format',
    ]),
    { message: /vcs-origin must be in format 'provider\/owner\/repo'/ }
  )
})

t.test('circleci with vcs-origin containing scheme prefix', async t => {
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
    npm.exec('trust', [
      'circleci',
      packageName,
      '--yes',
      '--org-id', '550e8400-e29b-41d4-a716-446655440000',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      '--vcs-origin', 'https://github.com/owner/repo',
    ]),
    { message: /vcs-origin must not include a scheme/ }
  )
})

t.test('circleci missing package name', async t => {
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
    npm.exec('trust', [
      'circleci',
      '--yes',
      '--org-id', '550e8400-e29b-41d4-a716-446655440000',
      '--project-id', '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      '--pipeline-definition-id', '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      '--vcs-origin', 'github.com/owner/repo',
    ]),
    { message: /Package name must be specified either as an argument or in package.json file/ }
  )
})

t.test('bodyToOptions with all fields', t => {
  const TrustCircleCI = require('../../../../lib/commands/trust/circleci.js')

  const body = {
    id: 'test-id',
    type: 'circleci',
    claims: {
      'oidc.circleci.com/org-id': '550e8400-e29b-41d4-a716-446655440000',
      'oidc.circleci.com/project-id': '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      'oidc.circleci.com/pipeline-definition-id': '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      'oidc.circleci.com/vcs-origin': 'github.com/owner/repo',
      'oidc.circleci.com/context-ids': ['123e4567-e89b-12d3-a456-426614174000'],
    },
  }

  const options = TrustCircleCI.bodyToOptions(body)

  t.equal(options.id, 'test-id', 'id should be set')
  t.equal(options.type, 'circleci', 'type should be set')
  t.equal(options.orgId, '550e8400-e29b-41d4-a716-446655440000', 'orgId should be set')
  t.equal(options.projectId, '7c9e6679-7425-40de-944b-e07fc1f90ae7', 'projectId should be set')
  t.equal(options.pipelineDefinitionId, '6ba7b810-9dad-11d1-80b4-00c04fd430c8', 'pipelineDefinitionId should be set')
  t.equal(options.vcsOrigin, 'github.com/owner/repo', 'vcsOrigin should be set')
  t.same(options.contextIds, ['123e4567-e89b-12d3-a456-426614174000'], 'contextIds should be set')
  t.end()
})

t.test('bodyToOptions without optional context_ids', t => {
  const TrustCircleCI = require('../../../../lib/commands/trust/circleci.js')

  const body = {
    id: 'test-id',
    type: 'circleci',
    claims: {
      'oidc.circleci.com/org-id': '550e8400-e29b-41d4-a716-446655440000',
      'oidc.circleci.com/project-id': '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      'oidc.circleci.com/pipeline-definition-id': '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
      'oidc.circleci.com/vcs-origin': 'github.com/owner/repo',
    },
  }

  const options = TrustCircleCI.bodyToOptions(body)

  t.equal(options.contextIds, undefined, 'contextIds should be undefined')
  t.end()
})

t.test('optionsToBody with all fields', t => {
  const TrustCircleCI = require('../../../../lib/commands/trust/circleci.js')

  const options = {
    orgId: '550e8400-e29b-41d4-a716-446655440000',
    projectId: '7c9e6679-7425-40de-944b-e07fc1f90ae7',
    pipelineDefinitionId: '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    vcsOrigin: 'github.com/owner/repo',
    contextIds: ['123e4567-e89b-12d3-a456-426614174000'],
  }

  const body = TrustCircleCI.optionsToBody(options)

  t.equal(body.type, 'circleci', 'type should be circleci')
  t.equal(body.claims['oidc.circleci.com/org-id'], '550e8400-e29b-41d4-a716-446655440000', 'org-id should be set')
  t.equal(body.claims['oidc.circleci.com/project-id'], '7c9e6679-7425-40de-944b-e07fc1f90ae7', 'project-id should be set')
  t.equal(body.claims['oidc.circleci.com/pipeline-definition-id'], '6ba7b810-9dad-11d1-80b4-00c04fd430c8', 'pipeline-definition-id should be set')
  t.equal(body.claims['oidc.circleci.com/vcs-origin'], 'github.com/owner/repo', 'vcs-origin should be set')
  t.same(body.claims['oidc.circleci.com/context-ids'], ['123e4567-e89b-12d3-a456-426614174000'], 'context-ids should be set')
  t.end()
})

t.test('optionsToBody without optional contextIds', t => {
  const TrustCircleCI = require('../../../../lib/commands/trust/circleci.js')

  const options = {
    orgId: '550e8400-e29b-41d4-a716-446655440000',
    projectId: '7c9e6679-7425-40de-944b-e07fc1f90ae7',
    pipelineDefinitionId: '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    vcsOrigin: 'github.com/owner/repo',
  }

  const body = TrustCircleCI.optionsToBody(options)

  t.equal(body.claims['oidc.circleci.com/context-ids'], undefined, 'context-ids should be undefined')
  t.end()
})

t.test('optionsToBody with multiple contextIds', t => {
  const TrustCircleCI = require('../../../../lib/commands/trust/circleci.js')

  const options = {
    orgId: '550e8400-e29b-41d4-a716-446655440000',
    projectId: '7c9e6679-7425-40de-944b-e07fc1f90ae7',
    pipelineDefinitionId: '6ba7b810-9dad-11d1-80b4-00c04fd430c8',
    vcsOrigin: 'github.com/owner/repo',
    contextIds: [
      '123e4567-e89b-12d3-a456-426614174000',
      'a1b2c3d4-e5f6-7890-abcd-ef1234567890',
    ],
  }

  const body = TrustCircleCI.optionsToBody(options)

  t.same(body.claims['oidc.circleci.com/context-ids'], [
    '123e4567-e89b-12d3-a456-426614174000',
    'a1b2c3d4-e5f6-7890-abcd-ef1234567890',
  ], 'context-ids should contain both UUIDs')
  t.end()
})

t.test('getVcsOriginUrl generates correct URL', t => {
  const TrustCircleCI = require('../../../../lib/commands/trust/circleci.js')

  t.equal(
    TrustCircleCI.prototype.getVcsOriginUrl('github.com/npm/cli'),
    'https://github.com/npm/cli',
    'should generate https URL from vcs-origin'
  )
  t.equal(
    TrustCircleCI.prototype.getVcsOriginUrl('bitbucket.org/owner/repo'),
    'https://bitbucket.org/owner/repo',
    'should work with bitbucket'
  )
  t.equal(
    TrustCircleCI.prototype.getVcsOriginUrl(null),
    null,
    'should return null for null input'
  )
  t.equal(
    TrustCircleCI.prototype.getVcsOriginUrl(undefined),
    null,
    'should return null for undefined input'
  )
  t.end()
})
