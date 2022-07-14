const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

let result = ''
const libnpmteam = {
  async add () {},
  async create () {},
  async destroy () {},
  async lsTeams () {},
  async lsUsers () {},
  async rm () {},
}
const npm = mockNpm({
  flatOptions: {},
  config: {
    loglevel: 'info',
  },
  output: (...msg) => {
    result += msg.join('\n')
  },
})
const mocks = {
  libnpmteam,
  'cli-columns': a => a.join(' '),
}

t.afterEach(() => {
  result = ''
  npm.flatOptions = {}
  npm.config.set('loglevel', 'info')
})

const Team = t.mock('../../../lib/commands/team.js', mocks)
const team = new Team(npm)

t.test('no args', async t => {
  await t.rejects(
    team.exec([]),
    'usage instructions',
    'should throw usage instructions'
  )
})

t.test('team add <scope:team> <user>', async t => {
  t.test('default output', async t => {
    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.matchSnapshot(result, 'should output success result for add user')
  })

  t.test('--parseable', async t => {
    npm.flatOptions.parseable = true

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.matchSnapshot(
      result,
      'should output success result for parseable add user'
    )
  })

  t.test('--json', async t => {
    npm.flatOptions.json = true

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.same(
      JSON.parse(result),
      {
        added: true,
        team: 'npmcli:developers',
        user: 'foo',
      },
      'should output success result for add user json'
    )
  })

  t.test('--silent', async t => {
    npm.config.set('loglevel', 'silent')

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.same(result, '', 'should not output success if silent')
  })
})

t.test('team create <scope:team>', async t => {
  t.test('default output', async t => {
    await team.exec(['create', '@npmcli:newteam'])

    t.matchSnapshot(result, 'should output success result for create team')
  })

  t.test('--parseable', async t => {
    npm.flatOptions.parseable = true

    await team.exec(['create', '@npmcli:newteam'])

    t.matchSnapshot(
      result,
      'should output parseable success result for create team'
    )
  })

  t.test('--json', async t => {
    npm.flatOptions.json = true

    await team.exec(['create', '@npmcli:newteam'])

    t.same(
      JSON.parse(result),
      {
        created: true,
        team: 'npmcli:newteam',
      },
      'should output success result for create team'
    )
  })

  t.test('--silent', async t => {
    npm.config.set('loglevel', 'silent')

    await team.exec(['create', '@npmcli:newteam'])

    t.same(result, '', 'should not output create success if silent')
  })
})

t.test('team destroy <scope:team>', async t => {
  t.test('default output', async t => {
    await team.exec(['destroy', '@npmcli:newteam'])
    t.matchSnapshot(result, 'should output success result for destroy team')
  })

  t.test('--parseable', async t => {
    npm.flatOptions.parseable = true
    await team.exec(['destroy', '@npmcli:newteam'])
    t.matchSnapshot(result, 'should output parseable result for destroy team')
  })

  t.test('--json', async t => {
    npm.flatOptions.json = true
    await team.exec(['destroy', '@npmcli:newteam'])
    t.same(
      JSON.parse(result),
      {
        deleted: true,
        team: 'npmcli:newteam',
      },
      'should output parseable result for destroy team'
    )
  })

  t.test('--silent', async t => {
    npm.config.set('loglevel', 'silent')
    await team.exec(['destroy', '@npmcli:newteam'])
    t.same(result, '', 'should not output destroy if silent')
  })
})

t.test('team ls <scope>', async t => {
  const libnpmteam = {
    async lsTeams () {
      return [
        'npmcli:developers',
        'npmcli:designers',
        'npmcli:product',
      ]
    },
  }

  const Team = t.mock('../../../lib/commands/team.js', {
    ...mocks,
    libnpmteam,
  })
  const team = new Team(npm)

  t.test('default output', async t => {
    await team.exec(['ls', '@npmcli'])
    t.matchSnapshot(result, 'should list teams for a given scope')
  })

  t.test('--parseable', async t => {
    npm.flatOptions.parseable = true
    await team.exec(['ls', '@npmcli'])
    t.matchSnapshot(result, 'should list teams for a parseable scope')
  })

  t.test('--json', async t => {
    npm.flatOptions.json = true
    await team.exec(['ls', '@npmcli'])
    t.same(
      JSON.parse(result),
      [
        'npmcli:designers',
        'npmcli:developers',
        'npmcli:product',
      ],
      'should json list teams for a scope json'
    )
  })

  t.test('--silent', async t => {
    npm.config.set('loglevel', 'silent')
    await team.exec(['ls', '@npmcli'])
    t.same(result, '', 'should not list teams if silent')
  })

  t.test('no teams', async t => {
    const libnpmteam = {
      async lsTeams () {
        return []
      },
    }

    const Team = t.mock('../../../lib/commands/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    await team.exec(['ls', '@npmcli'])

    t.matchSnapshot(result, 'should list no teams for a given scope')
  })

  t.test('single team', async t => {
    const libnpmteam = {
      async lsTeams () {
        return ['npmcli:developers']
      },
    }

    const Team = t.mock('../../../lib/commands/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    await team.exec(['ls', '@npmcli'])
    t.matchSnapshot(result, 'should list single team for a given scope')
  })
})

t.test('team ls <scope:team>', async t => {
  const libnpmteam = {
    async lsUsers () {
      return ['nlf', 'ruyadorno', 'darcyclarke', 'isaacs']
    },
  }
  const Team = t.mock('../../../lib/commands/team.js', {
    ...mocks,
    libnpmteam,
  })
  const team = new Team(npm)

  t.test('default output', async t => {
    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result, 'should list users for a given scope:team')
  })

  t.test('--parseable', async t => {
    npm.flatOptions.parseable = true
    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result, 'should list users for a parseable scope:team')
  })

  t.test('--json', async t => {
    npm.flatOptions.json = true
    await team.exec(['ls', '@npmcli:developers'])
    t.same(
      JSON.parse(result),
      [
        'darcyclarke',
        'isaacs',
        'nlf',
        'ruyadorno',
      ],
      'should list users for a scope:team json'
    )
  })

  t.test('--silent', async t => {
    npm.config.set('loglevel', 'silent')
    await team.exec(['ls', '@npmcli:developers'])
    t.same(result, '', 'should not output users if silent')
  })

  t.test('no users', async t => {
    const libnpmteam = {
      async lsUsers () {
        return []
      },
    }

    const Team = t.mock('../../../lib/commands/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result, 'should list no users for a given scope')
  })

  t.test('single user', async t => {
    const libnpmteam = {
      async lsUsers () {
        return ['foo']
      },
    }

    const Team = t.mock('../../../lib/commands/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result, 'should list single user for a given scope')
  })
})

t.test('team rm <scope:team> <user>', async t => {
  t.test('default output', async t => {
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.matchSnapshot(result, 'should output success result for remove user')
  })

  t.test('--parseable', async t => {
    npm.flatOptions.parseable = true
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.matchSnapshot(result, 'should output parseable result for remove user')
  })

  t.test('--json', async t => {
    npm.flatOptions.json = true
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.same(
      JSON.parse(result),
      {
        removed: true,
        team: 'npmcli:newteam',
        user: 'foo',
      },
      'should output json result for remove user'
    )
  })

  t.test('--silent', async t => {
    npm.config.set('loglevel', 'silent')
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.same(result, '', 'should not output rm result if silent')
  })
})

t.test('completion', t => {
  const { completion } = team

  t.test('npm team autocomplete', async t => {
    const res = await completion({
      conf: {
        argv: {
          remain: ['npm', 'team'],
        },
      },
    })
    t.strictSame(
      res,
      ['create', 'destroy', 'add', 'rm', 'ls'],
      'should auto complete with subcommands'
    )
    t.end()
  })

  t.test('npm team <subcommand> autocomplete', async t => {
    for (const subcmd of ['create', 'destroy', 'add', 'rm', 'ls']) {
      const res = await completion({
        conf: {
          argv: {
            remain: ['npm', 'team', subcmd],
          },
        },
      })
      t.strictSame(
        res,
        [],
        `should not autocomplete ${subcmd} subcommand`
      )
    }
  })

  t.test('npm team unknown subcommand autocomplete', async t => {
    t.rejects(completion({ conf: { argv: { remain: ['npm', 'team', 'missing-subcommand'] } } }),
      { message: 'missing-subcommand not recognized' }, 'should throw a a not recognized error'
    )

    t.end()
  })

  t.end()
})
