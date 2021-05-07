const t = require('tap')

let result = ''
const libnpmteam = {
  async add () {},
  async create () {},
  async destroy () {},
  async lsTeams () {},
  async lsUsers () {},
  async rm () {},
}
const npm = {
  flatOptions: {},
  output: (...msg) => {
    result += msg.join('\n')
  },
}
const mocks = {
  libnpmteam,
  'cli-columns': a => a.join(' '),
  '../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  '../../lib/utils/usage.js': () => 'usage instructions',
}

t.afterEach(() => {
  result = ''
  npm.flatOptions = {}
})

const Team = t.mock('../../lib/team.js', mocks)
const team = new Team(npm)

t.test('no args', t => {
  team.exec([], err => {
    t.match(
      err,
      'usage instructions',
      'should throw usage instructions'
    )
    t.end()
  })
})

t.test('team add <scope:team> <user>', t => {
  t.test('default output', t => {
    team.exec(['add', '@npmcli:developers', 'foo'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should output success result for add user')
      t.end()
    })
  })

  t.test('--parseable', t => {
    npm.flatOptions.parseable = true

    team.exec(['add', '@npmcli:developers', 'foo'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output success result for parseable add user'
      )
      t.end()
    })
  })

  t.test('--json', t => {
    npm.flatOptions.json = true

    team.exec(['add', '@npmcli:developers', 'foo'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        {
          added: true,
          team: 'npmcli:developers',
          user: 'foo',
        },
        'should output success result for add user json'
      )
      t.end()
    })
  })

  t.test('--silent', t => {
    npm.flatOptions.silent = true

    team.exec(['add', '@npmcli:developers', 'foo'], err => {
      if (err)
        throw err

      t.same(result, '', 'should not output success if silent')
      t.end()
    })
  })

  t.end()
})

t.test('team create <scope:team>', t => {
  t.test('default output', t => {
    team.exec(['create', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should output success result for create team')
      t.end()
    })
  })

  t.test('--parseable', t => {
    npm.flatOptions.parseable = true

    team.exec(['create', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output parseable success result for create team'
      )
      t.end()
    })
  })

  t.test('--json', t => {
    npm.flatOptions.json = true

    team.exec(['create', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        {
          created: true,
          team: 'npmcli:newteam',
        },
        'should output success result for create team'
      )
      t.end()
    })
  })

  t.test('--silent', t => {
    npm.flatOptions.silent = true

    team.exec(['create', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.same(result, '', 'should not output create success if silent')
      t.end()
    })
  })

  t.end()
})

t.test('team destroy <scope:team>', t => {
  t.test('default output', t => {
    team.exec(['destroy', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should output success result for destroy team')
      t.end()
    })
  })

  t.test('--parseable', t => {
    npm.flatOptions.parseable = true

    team.exec(['destroy', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should output parseable result for destroy team')
      t.end()
    })
  })

  t.test('--json', t => {
    npm.flatOptions.json = true

    team.exec(['destroy', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        {
          deleted: true,
          team: 'npmcli:newteam',
        },
        'should output parseable result for destroy team'
      )
      t.end()
    })
  })

  t.test('--silent', t => {
    npm.flatOptions.silent = true

    team.exec(['destroy', '@npmcli:newteam'], err => {
      if (err)
        throw err

      t.same(result, '', 'should not output destroy if silent')
      t.end()
    })
  })

  t.end()
})

t.test('team ls <scope>', t => {
  const libnpmteam = {
    async lsTeams () {
      return [
        'npmcli:developers',
        'npmcli:designers',
        'npmcli:product',
      ]
    },
  }

  const Team = t.mock('../../lib/team.js', {
    ...mocks,
    libnpmteam,
  })
  const team = new Team(npm)

  t.test('default output', t => {
    team.exec(['ls', '@npmcli'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list teams for a given scope')
      t.end()
    })
  })

  t.test('--parseable', t => {
    npm.flatOptions.parseable = true

    team.exec(['ls', '@npmcli'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list teams for a parseable scope')
      t.end()
    })
  })

  t.test('--json', t => {
    npm.flatOptions.json = true

    team.exec(['ls', '@npmcli'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        [
          'npmcli:designers',
          'npmcli:developers',
          'npmcli:product',
        ],
        'should json list teams for a scope json'
      )
      t.end()
    })
  })

  t.test('--silent', t => {
    npm.flatOptions.silent = true

    team.exec(['ls', '@npmcli'], err => {
      if (err)
        throw err

      t.same(result, '', 'should not list teams if silent')
      t.end()
    })
  })

  t.test('no teams', t => {
    const libnpmteam = {
      async lsTeams () {
        return []
      },
    }

    const Team = t.mock('../../lib/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    team.exec(['ls', '@npmcli'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list no teams for a given scope')
      t.end()
    })
  })

  t.test('single team', t => {
    const libnpmteam = {
      async lsTeams () {
        return ['npmcli:developers']
      },
    }

    const Team = t.mock('../../lib/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    team.exec(['ls', '@npmcli'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list single team for a given scope')
      t.end()
    })
  })

  t.end()
})

t.test('team ls <scope:team>', t => {
  const libnpmteam = {
    async lsUsers () {
      return ['nlf', 'ruyadorno', 'darcyclarke', 'isaacs']
    },
  }
  const Team = t.mock('../../lib/team.js', {
    ...mocks,
    libnpmteam,
  })
  const team = new Team(npm)

  t.test('default output', t => {
    team.exec(['ls', '@npmcli:developers'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list users for a given scope:team')
      t.end()
    })
  })

  t.test('--parseable', t => {
    npm.flatOptions.parseable = true

    team.exec(['ls', '@npmcli:developers'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list users for a parseable scope:team')
      t.end()
    })
  })

  t.test('--json', t => {
    npm.flatOptions.json = true

    team.exec(['ls', '@npmcli:developers'], err => {
      if (err)
        throw err

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
      t.end()
    })
  })

  t.test('--silent', t => {
    npm.flatOptions.silent = true

    team.exec(['ls', '@npmcli:developers'], err => {
      if (err)
        throw err

      t.same(result, '', 'should not output users if silent')
      t.end()
    })
  })

  t.test('no users', t => {
    const libnpmteam = {
      async lsUsers () {
        return []
      },
    }

    const Team = t.mock('../../lib/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    team.exec(['ls', '@npmcli:developers'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list no users for a given scope')
      t.end()
    })
  })

  t.test('single user', t => {
    const libnpmteam = {
      async lsUsers () {
        return ['foo']
      },
    }

    const Team = t.mock('../../lib/team.js', {
      ...mocks,
      libnpmteam,
    })
    const team = new Team(npm)

    team.exec(['ls', '@npmcli:developers'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should list single user for a given scope')
      t.end()
    })
  })

  t.end()
})

t.test('team rm <scope:team> <user>', t => {
  t.test('default output', t => {
    team.exec(['rm', '@npmcli:newteam', 'foo'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should output success result for remove user')
      t.end()
    })
  })

  t.test('--parseable', t => {
    npm.flatOptions.parseable = true

    team.exec(['rm', '@npmcli:newteam', 'foo'], err => {
      if (err)
        throw err

      t.matchSnapshot(result, 'should output parseable result for remove user')
      t.end()
    })
  })

  t.test('--json', t => {
    npm.flatOptions.json = true

    team.exec(['rm', '@npmcli:newteam', 'foo'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        {
          removed: true,
          team: 'npmcli:newteam',
          user: 'foo',
        },
        'should output json result for remove user'
      )
      t.end()
    })
  })

  t.test('--silent', t => {
    npm.flatOptions.silent = true

    team.exec(['rm', '@npmcli:newteam', 'foo'], err => {
      if (err)
        throw err

      t.same(result, '', 'should not output rm result if silent')
      t.end()
    })
  })

  t.end()
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
    t.rejects(completion({conf: {argv: {remain: ['npm', 'team', 'missing-subcommand'] } } }),
      {message: 'missing-subcommand not recognized'}, 'should throw a a not recognized error'
    )

    t.end()
  })

  t.end()
})
