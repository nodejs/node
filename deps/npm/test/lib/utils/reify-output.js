const t = require('tap')

const log = require('npmlog')
log.level = 'warn'

t.cleanSnapshot = str => str.replace(/in [0-9]+m?s/g, 'in {TIME}')

const settings = {
  fund: true,
}
const npm = {
  started: Date.now(),
  flatOptions: settings,
}
const reifyOutput = require('../../../lib/utils/reify-output.js')
t.test('missing info', (t) => {
  t.plan(1)
  npm.output = out => t.notMatch(
    out,
    'looking for funding',
    'should not print fund message if missing info'
  )

  reifyOutput(npm, {
    actualTree: {
      children: [],
    },
    diff: {
      children: [],
    },
  })
})

t.test('even more missing info', t => {
  t.plan(1)
  npm.output = out => t.notMatch(
    out,
    'looking for funding',
    'should not print fund message if missing info'
  )

  reifyOutput(npm, {
    actualTree: {
      children: [],
    },
  })
})

t.test('single package', (t) => {
  t.plan(1)
  npm.output = out => {
    if (out.endsWith('looking for funding')) {
      t.match(
        out,
        '1 package is looking for funding',
        'should print single package message'
      )
    }
  }

  reifyOutput(npm, {
    // a report with an error is the same as no report at all, if
    // the command is not 'audit'
    auditReport: {
      error: {
        message: 'no audit for youuuuu',
      },
    },
    actualTree: {
      name: 'foo',
      package: {
        name: 'foo',
        version: '1.0.0',
      },
      edgesOut: new Map([
        ['bar', {
          to: {
            name: 'bar',
            package: {
              name: 'bar',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' },
            },
          },
        }],
      ]),
    },
    diff: {
      children: [],
    },
  })
})

t.test('no message when funding config is false', (t) => {
  t.teardown(() => {
    settings.fund = true
  })
  settings.fund = false
  npm.output = out => {
    if (out.endsWith('looking for funding'))
      t.fail('should not print funding info', { actual: out })
  }

  reifyOutput(npm, {
    actualTree: {
      name: 'foo',
      package: {
        name: 'foo',
        version: '1.0.0',
      },
      edgesOut: new Map([
        ['bar', {
          to: {
            name: 'bar',
            package: {
              name: 'bar',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' },
            },
          },
        }],
      ]),
    },
    diff: {
      children: [],
    },
  })

  t.end()
})

t.test('print appropriate message for many packages', (t) => {
  t.plan(1)
  npm.output = out => {
    if (out.endsWith('looking for funding')) {
      t.match(
        out,
        '3 packages are looking for funding',
        'should print single package message'
      )
    }
  }

  reifyOutput(npm, {
    actualTree: {
      name: 'foo',
      package: {
        name: 'foo',
        version: '1.0.0',
      },
      edgesOut: new Map([
        ['bar', {
          to: {
            name: 'bar',
            package: {
              name: 'bar',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' },
            },
          },
        }],
        ['lorem', {
          to: {
            name: 'lorem',
            package: {
              name: 'lorem',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' },
            },
          },
        }],
        ['ipsum', {
          to: {
            name: 'ipsum',
            package: {
              name: 'ipsum',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' },
            },
          },
        }],
      ]),
    },
    diff: {
      children: [],
    },
  })
})

t.test('no output when silent', t => {
  npm.output = out => {
    t.fail('should not get output when silent', { actual: out })
  }
  t.teardown(() => log.level = 'warn')
  log.level = 'silent'
  reifyOutput(npm, {
    actualTree: { inventory: { size: 999 }, children: [] },
    auditReport: {
      toJSON: () => {
        throw new Error('this should not get called')
      },
      vulnerabilities: {},
      metadata: {
        vulnerabilities: {
          total: 99,
        },
      },
    },
    diff: {
      children: [
        { action: 'ADD', ideal: { location: 'loc' } },
      ],
    },
  })
  t.end()
})

t.test('packages changed message', t => {
  const output = []
  npm.output = out => {
    output.push(out)
  }

  // return a test function that builds up the mock and snapshots output
  const testCase = (t, added, removed, changed, audited, json, command) => {
    settings.json = json
    npm.command = command
    const mock = {
      actualTree: {
        inventory: { size: audited, has: () => true },
        children: [],
      },
      auditReport: audited ? {
        toJSON: () => mock.auditReport,
        vulnerabilities: {},
        metadata: {
          vulnerabilities: {
            total: 0,
          },
        },
      } : null,
      diff: {
        children: [
          { action: 'some random unexpected junk' },
        ],
      },
    }
    for (let i = 0; i < added; i++)
      mock.diff.children.push({ action: 'ADD', ideal: { location: 'loc' } })

    for (let i = 0; i < removed; i++)
      mock.diff.children.push({ action: 'REMOVE', actual: { location: 'loc' } })

    for (let i = 0; i < changed; i++) {
      const actual = { location: 'loc' }
      const ideal = { location: 'loc' }
      mock.diff.children.push({ action: 'CHANGE', actual, ideal })
    }
    output.length = 0
    reifyOutput(npm, mock)
    t.matchSnapshot(output.join('\n'), JSON.stringify({
      added,
      removed,
      changed,
      audited,
      json,
    }))
  }

  const cases = []
  for (const added of [0, 1, 2]) {
    for (const removed of [0, 1, 2]) {
      for (const changed of [0, 1, 2]) {
        for (const audited of [0, 1, 2]) {
          for (const json of [true, false])
            cases.push([added, removed, changed, audited, json, 'install'])
        }
      }
    }
  }

  // add case for when audit is the command
  cases.push([0, 0, 0, 2, true, 'audit'])
  cases.push([0, 0, 0, 2, false, 'audit'])

  t.plan(cases.length)
  for (const [added, removed, changed, audited, json, command] of cases)
    testCase(t, added, removed, changed, audited, json, command)

  t.end()
})

t.test('added packages should be looked up within returned tree', t => {
  t.test('has added pkg in inventory', t => {
    t.plan(1)
    npm.output = out => t.matchSnapshot(out)

    reifyOutput(npm, {
      actualTree: {
        name: 'foo',
        inventory: {
          has: () => true,
        },
      },
      diff: {
        children: [
          { action: 'ADD', ideal: { name: 'baz' } },
        ],
      },
    })
  })

  t.test('missing added pkg in inventory', t => {
    t.plan(1)
    npm.output = out => t.matchSnapshot(out)

    reifyOutput(npm, {
      actualTree: {
        name: 'foo',
        inventory: {
          has: () => false,
        },
      },
      diff: {
        children: [
          { action: 'ADD', ideal: { name: 'baz' } },
        ],
      },
    })
  })
  t.end()
})
