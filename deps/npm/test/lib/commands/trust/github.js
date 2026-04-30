const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')
const realProcLog = require('proc-log')

const packageName = '@npmcli/test-package'

t.test('github with all options provided', async t => {
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

  await npm.exec('trust', ['github', packageName, '--yes', '--file', 'workflow.yml', '--repository', 'owner/repo', '--environment', 'production'])
})

t.test('github with invalid repository format', async t => {
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

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--yes', '--file', 'workflow.yml', '--repository', 'invalid']),
    { message: /must be specified in the format owner\/repository/ }
  )
})

t.test('github with file as path', async t => {
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

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--yes', '--file', '.github/workflows/ci.yml', '--repository', 'owner/repo']),
    { message: /must be just a file not a path/ }
  )
})

t.test('github without environment', async t => {
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

  await npm.exec('trust', ['github', packageName, '--yes', '--file', 'workflow.yml', '--repository', 'owner/repo'])
})

t.test('bodyToOptions with all fields', t => {
  const TrustGitHub = require('../../../../lib/commands/trust/github.js')

  const body = {
    id: 'test-id',
    type: 'github',
    claims: {
      repository: 'owner/repo',
      workflow_ref: {
        file: 'test.yml',
      },
      environment: 'prod',
    },
  }

  const options = TrustGitHub.bodyToOptions(body)

  t.equal(options.id, 'test-id', 'id should be set')
  t.equal(options.type, 'github', 'type should be set')
  t.equal(options.file, 'test.yml', 'file should be set')
  t.equal(options.repository, 'owner/repo', 'repository should be set')
  t.equal(options.environment, 'prod', 'environment should be set')
  t.end()
})
