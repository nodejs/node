const t = require('tap')
const { load: _loadMockNpm } = require('../../fixtures/mock-npm')

t.test('should audit using Arborist', async t => {
  let ARB_ARGS = null
  let AUDIT_CALLED = false
  let REIFY_FINISH_CALLED = false
  let AUDIT_REPORT_CALLED = false
  let ARB_OBJ = null

  const loadMockNpm = (t) => _loadMockNpm(t, {
    mocks: {
      'npm-audit-report': () => {
        AUDIT_REPORT_CALLED = true
        return {
          report: 'there are vulnerabilities',
          exitCode: 0,
        }
      },
      '@npmcli/arborist': function (args) {
        ARB_ARGS = args
        ARB_OBJ = this
        this.audit = () => {
          AUDIT_CALLED = true
          this.auditReport = {}
        }
      },
      '../../lib/utils/reify-finish.js': (npm, arb) => {
        if (arb !== ARB_OBJ) {
          throw new Error('got wrong object passed to reify-output')
        }

        REIFY_FINISH_CALLED = true
      },
    },
  })

  t.test('audit', async t => {
    const { npm, outputs } = await loadMockNpm(t)
    await npm.exec('audit', [])
    t.match(ARB_ARGS, { audit: true, path: npm.prefix })
    t.equal(AUDIT_CALLED, true, 'called audit')
    t.equal(AUDIT_REPORT_CALLED, true, 'called audit report')
    t.match(outputs, [['there are vulnerabilities']])
  })

  t.test('audit fix', async t => {
    const { npm } = await loadMockNpm(t)
    await npm.exec('audit', ['fix'])
    t.equal(REIFY_FINISH_CALLED, true, 'called reify output')
  })
})

t.test('should audit - json', async t => {
  t.plan(1)
  const { npm } = await _loadMockNpm(t, {
    mocks: {
      'npm-audit-report': (_, opts) => {
        t.match(opts.reporter, 'json')
        return {
          report: 'there are vulnerabilities',
          exitCode: 0,
        }
      },
      '@npmcli/arborist': function () {
        this.audit = () => {
          this.auditReport = {}
        }
      },
      '../../lib/utils/reify-output.js': () => {},
    },
    config: {
      json: true,
    },
  })
  await npm.exec('audit', [])
})

t.test('report endpoint error', async t => {
  const loadMockNpm = (t, options) => _loadMockNpm(t, {
    mocks: {
      'npm-audit-report': () => {
        throw new Error('should not call audit report when there are errors')
      },
      '@npmcli/arborist': function () {
        this.audit = () => {
          this.auditReport = {
            error: {
              message: 'hello, this didnt work',
              method: 'POST',
              uri: 'https://example.com/',
              headers: {
                head: ['ers'],
              },
              statusCode: 420,
              body: 'this is a string',
            },
          }
        }
      },
      '../../lib/utils/reify-output.js': () => {},
    },
    ...options,
  })

  t.test('json=false', async t => {
    const { npm, outputs, logs } = await loadMockNpm(t, { config: { json: false } })
    await t.rejects(npm.exec('audit', []), 'audit endpoint returned an error')
    t.match(logs.warn, [['audit', 'hello, this didnt work']])
    t.strictSame(outputs, [['this is a string']])
  })

  t.test('json=true', async t => {
    const { npm, outputs, logs } = await loadMockNpm(t, { config: { json: true } })
    await t.rejects(npm.exec('audit', []), 'audit endpoint returned an error')
    t.match(logs.warn, [['audit', 'hello, this didnt work']])
    t.strictSame(outputs, [[
      '{\n' +
      '  "message": "hello, this didnt work",\n' +
      '  "method": "POST",\n' +
      '  "uri": "https://example.com/",\n' +
      '  "headers": {\n' +
      '    "head": [\n' +
      '      "ers"\n' +
      '    ]\n' +
      '  },\n' +
      '  "statusCode": 420,\n' +
      '  "body": "this is a string"\n' +
      '}',
    ],
    ])
  })
})

t.test('completion', async t => {
  const { npm } = await _loadMockNpm(t)
  const audit = await npm.cmd('audit')
  t.test('fix', async t => {
    await t.resolveMatch(
      audit.completion({ conf: { argv: { remain: ['npm', 'audit'] } } }),
      ['fix'],
      'completes to fix'
    )
  })

  t.test('subcommand fix', async t => {
    await t.resolveMatch(
      audit.completion({ conf: { argv: { remain: ['npm', 'audit', 'fix'] } } }),
      [],
      'resolves to ?'
    )
  })

  t.test('subcommand not recognized', async t => {
    await t.rejects(audit.completion({ conf: { argv: { remain: ['npm', 'audit', 'repare'] } } }), {
      message: 'repare not recognized',
    })
  })
})
