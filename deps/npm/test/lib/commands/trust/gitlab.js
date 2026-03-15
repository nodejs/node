const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')
const realProcLog = require('proc-log')

const packageName = '@npmcli/test-package'

t.test('gitlab with all options provided', async t => {
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

  await npm.exec('trust', ['gitlab', packageName, '--yes', '--file', '.gitlab-ci.yml', '--project', 'group/subgroup/repo', '--environment', 'production'])
})

t.test('gitlab with invalid project format', async t => {
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
    npm.exec('trust', ['gitlab', packageName, '--yes', '--file', '.gitlab-ci.yml', '--project', 'invalid']),
    { message: /must be specified in the format/ }
  )
})

t.test('gitlab with file as path', async t => {
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
    npm.exec('trust', ['gitlab', packageName, '--yes', '--file', '.gitlab/ci.yml', '--project', 'group/repo']),
    { message: /must be just a file not a path/ }
  )
})

t.test('gitlab without environment', async t => {
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

  await npm.exec('trust', ['gitlab', packageName, '--yes', '--file', '.gitlab-ci.yml', '--project', 'group/repo'])
})

t.test('bodyToOptions with all fields', t => {
  const TrustGitLab = require('../../../../lib/commands/trust/gitlab.js')

  const body = {
    id: 'test-id',
    type: 'gitlab',
    claims: {
      project_path: 'group/repo',
      ci_config_ref_uri: {
        file: '.gitlab-ci.yml',
      },
      environment: 'prod',
    },
  }

  const options = TrustGitLab.bodyToOptions(body)

  t.equal(options.id, 'test-id', 'id should be set')
  t.equal(options.type, 'gitlab', 'type should be set')
  t.equal(options.file, '.gitlab-ci.yml', 'file should be set')
  t.equal(options.project, 'group/repo', 'project should be set')
  t.equal(options.environment, 'prod', 'environment should be set')
  t.end()
})
