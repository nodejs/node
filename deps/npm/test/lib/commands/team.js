const t = require('tap')
const mockNpm = require('../../fixtures/mock-npm')

t.cleanSnapshot = s => s.trim().replace(/\n+/g, '\n')

const mockTeam = async (t, { libnpmteam, ...opts } = {}) => {
  const mock = await mockNpm(t, {
    ...opts,
    command: 'team',
    mocks: {
      // XXX: this should be refactored to use the mock registry
      libnpmteam: libnpmteam || {
        async add () {},
        async create () {},
        async destroy () {},
        async lsTeams () {},
        async lsUsers () {},
        async rm () {},
      },
    },
  })

  return {
    ...mock,
    result: () => mock.joinedOutput(),
  }
}

t.test('no args', async t => {
  const { team } = await mockTeam(t)
  await t.rejects(
    team.exec([]),
    'usage instructions',
    'should throw usage instructions'
  )
})

t.test('team add <scope:team> <user>', async t => {
  t.test('default output', async t => {
    const { team, result } = await mockTeam(t)

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.matchSnapshot(result(), 'should output success result for add user')
  })

  t.test('--parseable', async t => {
    const { team, result } = await mockTeam(t, {
      config: { parseable: true },
    })

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.matchSnapshot(
      result(),
      'should output success result for parseable add user'
    )
  })

  t.test('--json', async t => {
    const { team, result } = await mockTeam(t, {
      config: { json: true },
    })

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.same(
      JSON.parse(result()),
      {
        added: true,
        team: 'npmcli:developers',
        user: 'foo',
      },
      'should output success result for add user json'
    )
  })

  t.test('--silent', async t => {
    const { team, result } = await mockTeam(t, {
      config: { silent: true },
    })

    await team.exec(['add', '@npmcli:developers', 'foo'])

    t.same(result(), '', 'should not output success if silent')
  })
})

t.test('team create <scope:team>', async t => {
  t.test('default output', async t => {
    const { team, result } = await mockTeam(t)

    await team.exec(['create', '@npmcli:newteam'])

    t.matchSnapshot(result(), 'should output success result for create team')
  })

  t.test('--parseable', async t => {
    const { team, result } = await mockTeam(t, {
      config: { parseable: true },
    })

    await team.exec(['create', '@npmcli:newteam'])

    t.matchSnapshot(
      result(),
      'should output parseable success result for create team'
    )
  })

  t.test('--json', async t => {
    const { team, result } = await mockTeam(t, {
      config: { json: true },
    })

    await team.exec(['create', '@npmcli:newteam'])

    t.same(
      JSON.parse(result()),
      {
        created: true,
        team: 'npmcli:newteam',
      },
      'should output success result for create team'
    )
  })

  t.test('--silent', async t => {
    const { team, result } = await mockTeam(t, {
      config: { silent: true },
    })

    await team.exec(['create', '@npmcli:newteam'])

    t.same(result(), '', 'should not output create success if silent')
  })
})

t.test('team destroy <scope:team>', async t => {
  t.test('default output', async t => {
    const { team, result } = await mockTeam(t)
    await team.exec(['destroy', '@npmcli:newteam'])
    t.matchSnapshot(result(), 'should output success result for destroy team')
  })

  t.test('--parseable', async t => {
    const { team, result } = await mockTeam(t, {
      config: { parseable: true },
    })
    await team.exec(['destroy', '@npmcli:newteam'])
    t.matchSnapshot(result(), 'should output parseable result for destroy team')
  })

  t.test('--json', async t => {
    const { team, result } = await mockTeam(t, {
      config: { json: true },
    })
    await team.exec(['destroy', '@npmcli:newteam'])
    t.same(
      JSON.parse(result()),
      {
        deleted: true,
        team: 'npmcli:newteam',
      },
      'should output parseable result for destroy team'
    )
  })

  t.test('--silent', async t => {
    const { team, result } = await mockTeam(t, {
      config: { silent: true },
    })
    await team.exec(['destroy', '@npmcli:newteam'])
    t.same(result(), '', 'should not output destroy if silent')
  })
})

t.test('team ls <scope>', async t => {
  const teams = {
    async lsTeams () {
      return [
        'npmcli:developers',
        'npmcli:designers',
        'npmcli:product',
      ]
    },
  }

  const noTeam = {
    async lsTeams () {
      return []
    },
  }

  const singleTeam = {
    async lsTeams () {
      return ['npmcli:developers']
    },
  }

  t.test('default output', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: teams,
    })
    await team.exec(['ls', '@npmcli'])
    t.matchSnapshot(result(), 'should list teams for a given scope')
  })

  t.test('--parseable', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: teams,
      config: { parseable: true },
    })
    await team.exec(['ls', '@npmcli'])
    t.matchSnapshot(result(), 'should list teams for a parseable scope')
  })

  t.test('--json', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: teams,
      config: { json: true },
    })
    await team.exec(['ls', '@npmcli'])
    t.same(
      JSON.parse(result()),
      [
        'npmcli:designers',
        'npmcli:developers',
        'npmcli:product',
      ],
      'should json list teams for a scope json'
    )
  })

  t.test('--silent', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: teams,
      config: { silent: true },
    })
    await team.exec(['ls', '@npmcli'])
    t.same(result(), '', 'should not list teams if silent')
  })

  t.test('no teams', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: noTeam,
    })

    await team.exec(['ls', '@npmcli'])

    t.matchSnapshot(result(), 'should list no teams for a given scope')
  })

  t.test('single team', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: singleTeam,
    })

    await team.exec(['ls', '@npmcli'])
    t.matchSnapshot(result(), 'should list single team for a given scope')
  })
})

t.test('team ls <scope:team>', async t => {
  const users = {
    async lsUsers () {
      return ['nlf', 'ruyadorno', 'darcyclarke', 'isaacs']
    },
  }

  const singleUser = {
    async lsUsers () {
      return ['foo']
    },
  }

  const noUsers = {
    async lsUsers () {
      return []
    },
  }

  t.test('default output', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: users,
    })
    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result(), 'should list users for a given scope:team')
  })

  t.test('--parseable', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: users,
      config: { parseable: true },
    })
    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result(), 'should list users for a parseable scope:team')
  })

  t.test('--json', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: users,
      config: { json: true },
    })
    await team.exec(['ls', '@npmcli:developers'])
    t.same(
      JSON.parse(result()),
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
    const { team, result } = await mockTeam(t, {
      libnpmteam: users,
      config: { silent: true },
    })
    await team.exec(['ls', '@npmcli:developers'])
    t.same(result(), '', 'should not output users if silent')
  })

  t.test('no users', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: noUsers,
    })

    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result(), 'should list no users for a given scope')
  })

  t.test('single user', async t => {
    const { team, result } = await mockTeam(t, {
      libnpmteam: singleUser,
    })

    await team.exec(['ls', '@npmcli:developers'])
    t.matchSnapshot(result(), 'should list single user for a given scope')
  })
})

t.test('team rm <scope:team> <user>', async t => {
  t.test('default output', async t => {
    const { team, result } = await mockTeam(t)
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.matchSnapshot(result(), 'should output success result for remove user')
  })

  t.test('--parseable', async t => {
    const { team, result } = await mockTeam(t, {
      config: { parseable: true },
    })
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.matchSnapshot(result(), 'should output parseable result for remove user')
  })

  t.test('--json', async t => {
    const { team, result } = await mockTeam(t, {
      config: { json: true },
    })
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.same(
      JSON.parse(result()),
      {
        removed: true,
        team: 'npmcli:newteam',
        user: 'foo',
      },
      'should output json result for remove user'
    )
  })

  t.test('--silent', async t => {
    const { team, result } = await mockTeam(t, {
      config: { silent: true },
    })
    await team.exec(['rm', '@npmcli:newteam', 'foo'])
    t.same(result(), '', 'should not output rm result if silent')
  })
})

t.test('completion', async t => {
  const { team } = await mockTeam(t)

  t.test('npm team autocomplete', async t => {
    const res = await team.completion({
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
      const res = await team.completion({
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
    t.rejects(
      team.completion({ conf: { argv: { remain: ['npm', 'team', 'missing-subcommand'] } } }),
      { message: 'missing-subcommand not recognized' }, 'should throw a a not recognized error'
    )

    t.end()
  })

  t.end()
})
