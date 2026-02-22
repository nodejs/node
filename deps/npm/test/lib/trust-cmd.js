const t = require('tap')
const { load: loadMockNpm } = require('../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')
const realProcLog = require('proc-log')
const TrustCommand = require('../../lib/trust-cmd.js')

const packageName = '@npmcli/test-package'

t.test('trust-cmd via trust github with read function called', async t => {
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
      read: {
        read: async () => 'y',
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({ packageName })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])
})

t.test('trust-cmd via trust github with all options', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      yes: true,
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({ packageName })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli', '--environment', 'production'])
})

t.test('trust-cmd via trust github infers from package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
        repository: 'https://github.com/npm/cli',
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

  await npm.exec('trust', ['github', '--yes', '--file', 'workflow.yml'])
})

t.test('trust-cmd via trust github with dry-run', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
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

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])

  t.ok(joinedOutput().includes('Establishing trust'), 'shows notice about establishing trust')
})

t.test('trust-cmd via trust github missing package name', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {},
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', '--file', 'workflow.yml', '--repository', 'npm/cli']),
    { message: /Package name must be specified/ },
    'throws when no package name'
  )
})

t.test('trust-cmd via trust github missing file', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--repository', 'npm/cli']),
    { message: /must be specified with the file option/ },
    'throws when no file'
  )
})

t.test('trust-cmd via trust github invalid file extension', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.txt', '--repository', 'npm/cli']),
    { message: /must end in \.yml or \.yaml/ },
    'throws when file has wrong extension'
  )
})

t.test('trust-cmd via trust github missing repository', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml']),
    { message: /must be specified with repository option/ },
    'throws when no repository'
  )
})

t.test('trust-cmd via trust github with custom registry warning', async t => {
  const { npm, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      registry: 'https://custom.registry.com/',
      'dry-run': true,
    },
  })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])

  t.ok(logs.warn.some(l => l.includes('may not support trusted publishing')), 'warns about custom registry')
})

t.test('trust-cmd via trust github with --json', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      json: true,
      'dry-run': true,
    },
  })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])

  const output = joinedOutput()
  t.ok(output.includes(packageName), 'JSON output includes package name')
  t.ok(output.includes('workflow.yml'), 'JSON output includes file')
})

t.test('trust-cmd via trust github with user confirmation no', async t => {
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
          read: async () => 'n',
        },
      },
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli']),
    { message: 'User cancelled operation' },
    'throws when user declines'
  )
})

t.test('trust-cmd via trust github with --no-yes', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      yes: false,
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli']),
    { message: 'User cancelled operation' },
    'throws when --no-yes flag is set'
  )
})

t.test('trust-cmd via trust github with invalid answer', async t => {
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
          read: async () => 'maybe',
        },
      },
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli']),
    { message: 'User cancelled operation' },
    'throws when user gives invalid answer'
  )
})

t.test('trust-cmd via trust github with user confirmation Y uppercase', async t => {
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
          read: async () => 'Y',
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

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])
})

t.test('trust-cmd via trust github with user enters empty string', async t => {
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
          read: async () => '',
        },
      },
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli']),
    { message: 'User cancelled operation' },
    'throws when user enters empty string'
  )
})

t.test('trust-cmd via trust github with mismatched repo type', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
        repository: 'https://gitlab.com/npm/cli',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', '--file', 'workflow.yml']),
    { message: /Repository in package.json is not a GitHub repository/ },
    'throws when repository type does not match provider'
  )
})

t.test('trust-cmd via trust github with mismatched repo type but flag provided', async t => {
  const { npm, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
        repository: 'https://gitlab.com/owner/old-repo',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      'dry-run': true,
    },
  })

  await npm.exec('trust', ['github', '--file', 'workflow.yml', '--repository', 'owner/new-repo'])

  t.ok(logs.warn.some(l => l.includes('Repository in package.json is not a GitHub repository')), 'warns about repository type mismatch')
})

t.test('trust-cmd via trust github with different repo in package.json', async t => {
  const { npm, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
        repository: 'https://github.com/owner/old-repo',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      'dry-run': true,
    },
  })

  await npm.exec('trust', ['github', '--file', 'workflow.yml', '--repository', 'owner/new-repo'])

  t.ok(logs.warn.some(l => l.includes('differs from provided')), 'warns about repository mismatch')
})

t.test('trust-cmd via trust github with user confirmation yes spelled out', async t => {
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
          read: async () => 'yes',
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

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])
})

t.test('trust-cmd via trust github showing response with id and type', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
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
      read: {
        read: async () => 'y',
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({
    packageName,
    body: {
      id: 'config-id-123',
      type: 'github',
      claims: {
        repository: 'npm/cli',
        workflow_ref: {
          file: 'workflow.yml',
        },
      },
    },
  })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])

  const output = joinedOutput()
  t.ok(output.includes('type:'), 'output shows type field')
  t.ok(output.includes('id:'), 'output shows id field')
})

t.test('trust-cmd via trust github missing repository when package name differs', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'other-package',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml']),
    { message: /must be specified with repository option/ },
    'throws when no repository and package name differs'
  )
})

t.test('TrustCommand - createConfig', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({ packageName })

  const response = await cmd.createConfig(packageName, [{ type: 'test' }])
  t.ok(response, 'returns a response')
})

t.test('TrustCommand - bodyToOptions', t => {
  const body = {
    id: 'test-id',
    type: 'test-type',
    other: 'ignored',
  }

  const options = TrustCommand.bodyToOptions(body)

  t.equal(options.id, 'test-id', 'includes id')
  t.equal(options.type, 'test-type', 'includes type')
  t.notOk(options.other, 'does not include other fields')
  t.end()
})

t.test('TrustCommand - bodyToOptions with missing fields', t => {
  const body = {}

  const options = TrustCommand.bodyToOptions(body)

  t.same(options, {}, 'returns empty object when no fields')
  t.end()
})

t.test('TrustCommand - NPM_FRONTEND constant', t => {
  t.equal(TrustCommand.NPM_FRONTEND, 'https://www.npmjs.com', 'exports NPM_FRONTEND constant')
  t.end()
})
t.test('trust-cmd via trust github showing fromPackageJson indicator', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
        repository: 'https://github.com/npm/cli',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
    mocks: {
      read: {
        read: async () => 'y',
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({
    packageName,
    body: {
      id: 'config-id-123',
      type: 'github',
      claims: {
        repository: 'npm/cli',
        workflow_ref: {
          file: 'workflow.yml',
        },
      },
    },
  })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml'])

  const output = joinedOutput()
  t.ok(output.includes('from package.json'), 'output shows fromPackageJson indicator')
})

t.test('trust-cmd via trust github showing URLs for fields', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
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
      read: {
        read: async () => 'y',
      },
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })

  registry.trustCreate({
    packageName,
    body: {
      id: 'config-id-123',
      type: 'github',
      claims: {
        repository: 'npm/cli',
        workflow_ref: {
          file: 'workflow.yml',
        },
      },
    },
  })

  await npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli'])

  const output = joinedOutput()
  t.match(output, /https:\/\/github\.com\/npm\/cli\b/, 'output shows repository URL')
})

t.test('trust-cmd via trust github with yes=false flag', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
      }),
    },
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
      yes: false,
    },
  })

  await t.rejects(
    npm.exec('trust', ['github', packageName, '--file', 'workflow.yml', '--repository', 'npm/cli']),
    { message: /User cancelled operation/ },
    'throws when yes is explicitly false'
  )
})

t.test('TrustCommand - logOptions with no values', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  // Call logOptions with no values object
  cmd.logOptions({})
  t.pass('logOptions handles missing values object')
})

t.test('TrustCommand - logOptions with falsey value', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  // Call logOptions with a falsey but not null/undefined value
  cmd.logOptions({ values: { type: 'test', falseyField: 0, anotherFalsey: false, emptyString: '' } })
  t.pass('logOptions handles falsey values that are not null/undefined')
})

t.test('TrustCommand - logOptions with null and undefined values', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  // Call logOptions with null and undefined values that should be skipped
  cmd.logOptions({ values: { type: 'test', id: 'test-id', nullField: null, undefinedField: undefined, validField: 'value' } })
  const output = joinedOutput()
  t.ok(output.includes('validField'), 'shows valid field')
  t.notOk(output.includes('nullField'), 'skips null field')
  t.notOk(output.includes('undefinedField'), 'skips undefined field')
})

t.test('TrustCommand - logOptions with fromPackageJson and urls', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  // Call logOptions with fromPackageJson and urls objects
  cmd.logOptions({
    values: {
      type: 'github',
      id: 'test-id',
      repository: 'npm/cli',
      file: 'workflow.yml',
    },
    fromPackageJson: {
      repository: true,
    },
    urls: {
      repository: 'https://github.com/npm/cli',
      file: 'https://github.com/npm/cli/-/blob/HEAD/workflow.yml',
    },
  })
  const output = joinedOutput()
  t.ok(output.includes('from package.json'), 'shows fromPackageJson indicator')
  t.match(output, /https:\/\/github\.com\/npm\/cli\b/, 'shows URL')
})

t.test('TrustCommand - logOptions with no urls', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  // Call logOptions without urls object
  cmd.logOptions({
    values: {
      type: 'github',
      id: 'test-id',
      repository: 'npm/cli',
      file: 'workflow.yml',
    },
  })
  const output = joinedOutput()
  t.ok(output.includes('repository'), 'shows repository field')
  t.ok(output.includes('file'), 'shows file field')
  t.notOk(output.includes('Links to verify manually'), 'does not show links header when no urls')
})

t.test('TrustCommand - logOptions with urls but all values are null', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'test-auth-token',
    },
  })

  class TestTrustCmd extends TrustCommand {
    static name = 'test'
    static description = 'Test command'
  }

  const cmd = new TestTrustCmd(npm)

  // Call logOptions with urls object but all values are null/undefined
  cmd.logOptions({
    values: {
      type: 'github',
      id: 'test-id',
      repository: 'npm/cli',
      file: 'workflow.yml',
    },
    urls: {
      repository: null,
      file: undefined,
    },
  })
  const output = joinedOutput()
  t.ok(output.includes('repository'), 'shows repository field')
  t.ok(output.includes('file'), 'shows file field')
  t.notOk(output.includes('Links to verify manually'), 'does not show links header when all urls are null')
})
