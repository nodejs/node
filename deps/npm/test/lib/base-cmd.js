const t = require('tap')
const { load: loadMockNpm } = require('../fixtures/mock-npm')
const BaseCommand = require('../../lib/base-cmd.js')
const Definition = require('@npmcli/config/lib/definitions/definition.js')

t.test('flags() method with command definitions', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      mountain: 'kilimanjaro',
    },
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['mountain']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.ok(flags, 'flags() returns an object')
  t.equal(flags.mountain, 'kilimanjaro', 'includes config value when set')
})

t.test('flags() method with default values', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['mountain']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.equal(flags.mountain, 'everest', 'uses default value when not set')
})

t.test('flags() method filters unknown options', async t => {
  const { npm } = await loadMockNpm(t, {
    // npm.config.argv would have both known and unknown flags parsed
    config: {
      mountain: 'denali',
    },
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['mountain']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.equal(flags.mountain, 'denali', 'includes known flag')
  t.notOk(flags.bug, 'filters out unknown flags')
  t.same(Object.keys(flags), ['mountain'], 'only includes defined keys')
})

t.test('flags() method with no definitions', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'

    async exec () {
      return this.flags()
    }
  }

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.same(flags, {}, 'returns empty object when no definitions')
})

t.test('flags() throws error for unknown flags', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['mountain']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Manually set config.argv to simulate command-line with unknown flag
  npm.config.argv = ['node', 'npm', 'test-command', '--unknown-flag']

  const command = new TestCommand(npm)
  await t.rejects(
    command.exec(),
    { message: /Unknown flag.*--unknown-flag/ },
    'throws error for unknown flag'
  )
})

t.test('flags() maps alias to main key', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['mountain']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
        alias: ['peak'],
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Use the alias --peak instead of --mountain
  npm.config.argv = ['node', 'npm', 'test-command', '--peak=denali']

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.equal(flags.mountain, 'denali', 'alias value is mapped to main key')
  t.notOk('peak' in flags, 'alias key is not present in flags')
})

t.test('flags() throws error when both main key and alias are provided', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['mountain']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
        alias: ['peak'],
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Provide both --mountain and --peak (its alias)
  npm.config.argv = ['node', 'npm', 'test-command', '--mountain=everest', '--peak=denali']

  const command = new TestCommand(npm)
  await t.rejects(
    command.exec(),
    { message: /Please provide only one of --mountain or --peak/ },
    'throws error when main key and alias are both provided'
  )
})

t.test('getUsage() with no params and no definitions', async t => {
  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command description'
  }

  const usage = TestCommand.describeUsage

  t.ok(usage.includes('Test command description'), 'includes description')
  t.ok(usage.includes('npm test-command'), 'includes usage line')
  t.notOk(usage.includes('Options:'), 'does not include Options section')
})

t.test('getUsage() with both params and definitions', async t => {
  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command description'
    static params = ['mountain', 'river']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
      new Definition('river', {
        type: String,
        default: 'nile',
        description: 'Your favorite river',
        usage: '--river=<river>',
      }),
    ]
  }

  const usage = TestCommand.describeUsage

  t.ok(usage.includes('Test command description'), 'includes description')
  t.ok(usage.includes('Options:'), 'includes Options section')
  t.ok(usage.includes('--mountain'), 'includes mountain flag')
  t.ok(usage.includes('--river'), 'includes river flag')
})

t.test('getUsage() with subcommand without description', async t => {
  class SubCommandWithDesc extends BaseCommand {
    static name = 'with-desc'
    static description = 'Subcommand with description'
  }

  class SubCommandNoDesc extends BaseCommand {
    static name = 'no-desc'
    // No description
  }

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command description'
    static subcommands = {
      'with-desc': SubCommandWithDesc,
      'no-desc': SubCommandNoDesc,
    }
  }

  const usage = TestCommand.describeUsage

  t.ok(usage.includes('Subcommands:'), 'includes Subcommands section')
  t.ok(usage.includes('with-desc'), 'includes subcommand with description')
  t.ok(usage.includes('Subcommand with description'), 'includes the description text')
  t.ok(usage.includes('no-desc'), 'includes subcommand without description')
})

t.test('getUsage() with definition without description', async t => {
  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command description'
    static params = ['mountain', 'river']

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
      new Definition('river', {
        type: String,
        default: 'nile',
        description: '', // Empty description
        usage: '--river=<river>',
      }),
    ]
  }

  const usage = TestCommand.describeUsage

  t.ok(usage.includes('Options:'), 'includes Options section')
  t.ok(usage.includes('--mountain'), 'includes mountain flag in options')
  t.ok(usage.includes('Your favorite mountain'), 'includes mountain description')
  t.ok(usage.includes('[--river=<river>]'), 'includes river in usage line')
  t.notOk(usage.includes('  --river'), 'does not include river flag description section')
})

t.test('flags() handles definition with multiple aliases', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
        alias: ['peak', 'summit'], // Multiple aliases
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Use the second alias --summit
  npm.config.argv = ['node', 'npm', 'test-command', '--summit=denali']

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.equal(flags.mountain, 'denali', 'second alias value is mapped to main key')
  t.notOk('summit' in flags, 'alias key is not present in flags')
  t.notOk('peak' in flags, 'other alias key is not present in flags')
})

t.test('flags() handles definition with short as array', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
        short: ['m', 'M'], // Short as array
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Use the short flag -m
  npm.config.argv = ['node', 'npm', 'test-command', '-m', 'denali']

  const command = new TestCommand(npm)
  const [flags] = await command.exec()

  t.equal(flags.mountain, 'denali', 'short flag value is parsed correctly')
})

t.test('flags() returns defaults when argv is empty', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Set argv to empty array
  npm.config.argv = []

  const command = new TestCommand(npm)
  const [flags, remains] = await command.exec()

  t.equal(flags.mountain, 'everest', 'returns default value when argv is empty')
  t.same(remains, [], 'remains is empty array')
})

t.test('flags() throws error for multiple unknown flags with pluralization', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'

    static definitions = [
      new Definition('mountain', {
        type: String,
        default: 'everest',
        description: 'Your favorite mountain',
        usage: '--mountain=<mountain>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Provide multiple unknown flags
  npm.config.argv = ['node', 'npm', 'test-command', '--unknown-one', '--unknown-two']

  const command = new TestCommand(npm)
  await t.rejects(
    command.exec(),
    { message: /Unknown flags:.*--unknown-one.*--unknown-two/ },
    'throws error with pluralized "flags" for multiple unknown flags'
  )
})

t.test('base exec() method returns undefined', async t => {
  const { npm } = await loadMockNpm(t)

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    // Intentionally not overriding exec() to test the base implementation
  }

  const command = new TestCommand(npm)
  const result = await command.exec()

  t.equal(result, undefined, 'base exec() returns undefined')
})

t.test('flags() removes unknown positional warning when value is consumed by command definition', async t => {
  // Pass raw argv to loadMockNpm so warnings are generated during config.load()
  // The global config sees --id as unknown (boolean), so "abc123" becomes a positional
  // and queues a warning. But command-specific definitions should consume it.
  const { npm, logs } = await loadMockNpm(t, {
    argv: ['--id', 'abc123'],
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['id']

    static definitions = [
      new Definition('id', {
        type: String,
        default: null,
        description: 'An identifier',
        usage: '--id=<id>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Set up argv for command execution (mock-npm prepends the command)
  npm.config.argv = ['node', 'npm', 'test-command', '--id', 'abc123']

  const command = new TestCommand(npm)
  const [flags, remains] = await command.exec()

  // The flag should be properly parsed
  t.equal(flags.id, 'abc123', 'id flag is properly parsed')
  t.same(remains, [], 'no remaining positionals')

  // Check that no warning about "abc123" being parsed as positional was logged
  const warningLogs = logs.warn
  const positionalWarnings = warningLogs.filter(msg =>
    msg.includes('abc123') && msg.includes('parsed as a normal command line argument')
  )
  t.equal(positionalWarnings.length, 0, 'no warning about abc123 being a positional')
})

t.test('flags() keeps unknown positional warning when multiple values follow unknown flag', async t => {
  // Pass raw argv to loadMockNpm so warnings are generated during config.load()
  // Both "abc123" and "def456" are seen as positionals by global config because --id is unknown
  // nopt only warns about "abc123" (the immediate next value after unknown flag)
  // Command definition consumes "abc123" for --id, "def456" remains as true positional
  const { npm, logs } = await loadMockNpm(t, {
    argv: ['--id', 'abc123', 'def456'],
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['id']

    static definitions = [
      new Definition('id', {
        type: String,
        default: null,
        description: 'An identifier',
        usage: '--id=<id>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Set up argv for command execution
  npm.config.argv = ['node', 'npm', 'test-command', '--id', 'abc123', 'def456']

  const command = new TestCommand(npm)
  const [flags, remains] = await command.exec()

  // The flag should be properly parsed
  t.equal(flags.id, 'abc123', 'id flag is properly parsed')
  t.same(remains, ['def456'], 'def456 remains as positional')

  // Check that warning about "abc123" was removed (consumed by --id)
  const warningLogs = logs.warn
  const abc123Warnings = warningLogs.filter(msg =>
    msg.includes('abc123') && msg.includes('parsed as a normal command line argument')
  )
  t.equal(abc123Warnings.length, 0, 'no warning about abc123 (consumed by --id)')
})

t.test('flags() does not remove unknown positional warning when value is in remains', async t => {
  // This tests the else branch where remainsSet.has(unknownPos) is true
  // When a value is a true positional (in remains), we should NOT remove its warning
  // The warning should be logged (not suppressed)
  const { npm, logs } = await loadMockNpm(t, {
    argv: ['truepositional'],
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static params = ['id']

    static definitions = [
      new Definition('id', {
        type: String,
        default: null,
        description: 'An identifier',
        usage: '--id=<id>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Manually queue a warning for a value that will be in remains
  npm.config.queueWarning('unknown:truepositional', 'config', 'truepositional was parsed as positional')

  // Set up argv for command execution with the value as a true positional
  npm.config.argv = ['node', 'npm', 'test-command', 'truepositional']

  const command = new TestCommand(npm)
  const [flags, remains] = await command.exec()

  // The positional should remain
  t.same(remains, ['truepositional'], 'truepositional is in remains')
  t.equal(flags.id, null, 'id flag uses default')

  // Check that the warning WAS logged (not removed before logWarnings())
  // Because the value is in remains, it's a true positional and should warn
  const warningLogs = logs.warn
  const positionalWarnings = warningLogs.filter(msg =>
    msg.includes('truepositional') && msg.includes('parsed as positional')
  )
  t.equal(positionalWarnings.length, 1, 'warning for truepositional was logged')
})

t.test('flags() throws error for extra positional arguments beyond expected count', async t => {
  // When a command specifies static positionals = N, extra positionals should throw an error
  const { npm } = await loadMockNpm(t, {
    argv: ['pkg1', 'extra1', 'extra2'],
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static positionals = 1 // expects only 1 positional
    static params = ['id']

    static definitions = [
      new Definition('id', {
        type: String,
        default: null,
        description: 'An identifier',
        usage: '--id=<id>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Set up argv for command execution with multiple positionals
  npm.config.argv = ['node', 'npm', 'test-command', 'pkg1', 'extra1', 'extra2']

  const command = new TestCommand(npm)

  // Should throw error for extra positional
  await t.rejects(
    command.exec(),
    { message: 'Unknown positional argument: extra1' },
    'throws error for first extra positional'
  )
})

t.test('flags() does not throw when positionals is null (unlimited)', async t => {
  // When static positionals is null, any number of positionals is allowed without error
  const { npm } = await loadMockNpm(t, {
    argv: ['pkg1', 'extra1', 'extra2'],
  })

  class TestCommand extends BaseCommand {
    static name = 'test-command'
    static description = 'Test command'
    static positionals = null // unlimited/unchecked
    static params = ['id']

    static definitions = [
      new Definition('id', {
        type: String,
        default: null,
        description: 'An identifier',
        usage: '--id=<id>',
      }),
    ]

    async exec () {
      return this.flags()
    }
  }

  // Set up argv for command execution with multiple positionals
  npm.config.argv = ['node', 'npm', 'test-command', 'pkg1', 'extra1', 'extra2']

  const command = new TestCommand(npm)
  const [flags, remains] = await command.exec()

  // All positionals should remain - no error thrown
  t.same(remains, ['pkg1', 'extra1', 'extra2'], 'all positionals are in remains')
  t.equal(flags.id, null, 'id flag uses default')
})
