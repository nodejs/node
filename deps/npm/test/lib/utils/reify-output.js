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

t.test('showing and not showing audit report', async t => {
  const auditReport = {
    toJSON: () => auditReport,
    auditReportVersion: 2,
    vulnerabilities: {
      minimist: {
        name: 'minimist',
        severity: 'low',
        via: [
          {
            id: 1179,
            url: 'https://npmjs.com/advisories/1179',
            title: 'Prototype Pollution',
            severity: 'low',
            vulnerable_versions: '<0.2.1 || >=1.0.0 <1.2.3',
          },
        ],
        effects: [],
        range: '<0.2.1 || >=1.0.0 <1.2.3',
        nodes: [
          'node_modules/minimist',
        ],
        fixAvailable: true,
      },
    },
    metadata: {
      vulnerabilities: {
        info: 0,
        low: 1,
        moderate: 0,
        high: 0,
        critical: 0,
        total: 1,
      },
      dependencies: {
        prod: 1,
        dev: 0,
        optional: 0,
        peer: 0,
        peerOptional: 0,
        total: 1,
      },
    },
  }

  t.test('no output when silent', t => {
    npm.output = out => {
      t.fail('should not get output when silent', { actual: out })
    }
    t.teardown(() => log.level = 'warn')
    log.level = 'silent'
    reifyOutput(npm, {
      actualTree: { inventory: { size: 999 }, children: [] },
      auditReport,
      diff: {
        children: [
          { action: 'ADD', ideal: { location: 'loc' } },
        ],
      },
    })
    t.end()
  })

  t.test('output when not silent', t => {
    const OUT = []
    npm.output = out => {
      OUT.push(out)
    }
    reifyOutput(npm, {
      actualTree: { inventory: new Map(), children: [] },
      auditReport,
      diff: {
        children: [
          { action: 'ADD', ideal: { location: 'loc' } },
        ],
      },
    })
    t.match(OUT.join('\n'), /Run `npm audit` for details\.$/, 'got audit report')
    t.end()
  })

  for (const json of [true, false]) {
    t.test(`json=${json}`, t => {
      t.teardown(() => {
        delete npm.flatOptions.json
      })
      npm.flatOptions.json = json
      t.test('set exit code when cmd is audit', t => {
        npm.output = () => {}
        const { exitCode } = process
        const { command } = npm
        npm.flatOptions.auditLevel = 'low'
        t.teardown(() => {
          delete npm.flatOptions.auditLevel
          npm.command = command
          // only set exitCode back if we're passing tests
          if (t.passing())
            process.exitCode = exitCode
        })

        process.exitCode = 0
        npm.command = 'audit'
        reifyOutput(npm, {
          actualTree: { inventory: new Map(), children: [] },
          auditReport,
          diff: {
            children: [
              { action: 'ADD', ideal: { location: 'loc' } },
            ],
          },
        })

        t.equal(process.exitCode, 1, 'set exit code')
        t.end()
      })

      t.test('do not set exit code when cmd is install', t => {
        npm.output = () => {}
        const { exitCode } = process
        const { command } = npm
        npm.flatOptions.auditLevel = 'low'
        t.teardown(() => {
          delete npm.flatOptions.auditLevel
          npm.command = command
          // only set exitCode back if we're passing tests
          if (t.passing())
            process.exitCode = exitCode
        })

        process.exitCode = 0
        npm.command = 'install'
        reifyOutput(npm, {
          actualTree: { inventory: new Map(), children: [] },
          auditReport,
          diff: {
            children: [
              { action: 'ADD', ideal: { location: 'loc' } },
            ],
          },
        })

        t.equal(process.exitCode, 0, 'did not set exit code')
        t.end()
      })
      t.end()
    })
  }

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
