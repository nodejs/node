const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

t.test('should audit using Arborist', t => {
  let ARB_ARGS = null
  let AUDIT_CALLED = false
  let REIFY_FINISH_CALLED = false
  let AUDIT_REPORT_CALLED = false
  let OUTPUT_CALLED = false
  let ARB_OBJ = null

  const npm = mockNpm({
    prefix: 'foo',
    config: {
      json: false,
    },
    output: () => {
      OUTPUT_CALLED = true
    },
  })
  const Audit = t.mock('../../lib/audit.js', {
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
      if (arb !== ARB_OBJ)
        throw new Error('got wrong object passed to reify-output')

      REIFY_FINISH_CALLED = true
    },
  })

  const audit = new Audit(npm)

  t.test('audit', t => {
    audit.exec([], () => {
      t.match(ARB_ARGS, { audit: true, path: 'foo' })
      t.equal(AUDIT_CALLED, true, 'called audit')
      t.equal(AUDIT_REPORT_CALLED, true, 'called audit report')
      t.equal(OUTPUT_CALLED, true, 'called audit report')
      t.end()
    })
  })

  t.test('audit fix', t => {
    audit.exec(['fix'], () => {
      t.equal(REIFY_FINISH_CALLED, true, 'called reify output')
      t.end()
    })
  })

  t.end()
})

t.test('should audit - json', t => {
  const npm = mockNpm({
    prefix: 'foo',
    config: {
      json: true,
    },
    output: () => {},
  })

  const Audit = t.mock('../../lib/audit.js', {
    'npm-audit-report': () => ({
      report: 'there are vulnerabilities',
      exitCode: 0,
    }),
    '@npmcli/arborist': function () {
      this.audit = () => {
        this.auditReport = {}
      }
    },
    '../../lib/utils/reify-output.js': () => {},
  })
  const audit = new Audit(npm)

  audit.exec([], (err) => {
    t.notOk(err, 'no errors')
    t.end()
  })
})

t.test('report endpoint error', t => {
  for (const json of [true, false]) {
    t.test(`json=${json}`, t => {
      const OUTPUT = []
      const LOGS = []
      const npm = mockNpm({
        prefix: 'foo',
        command: 'audit',
        config: {
          json,
        },
        flatOptions: {
          json,
        },
        log: {
          warn: (...warning) => LOGS.push(warning),
        },
        output: (...msg) => {
          OUTPUT.push(msg)
        },
      })
      const Audit = t.mock('../../lib/audit.js', {
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
                body: json ? { nope: 'lol' }
                : Buffer.from('i had a vuln but i eated it lol'),
              },
            }
          }
        },
        '../../lib/utils/reify-output.js': () => {},
      })
      const audit = new Audit(npm)

      audit.exec([], (err) => {
        t.equal(err, 'audit endpoint returned an error')
        t.strictSame(OUTPUT, [
          [
            json ? '{\n' +
              '  "message": "hello, this didnt work",\n' +
              '  "method": "POST",\n' +
              '  "uri": "https://example.com/",\n' +
              '  "headers": {\n' +
              '    "head": [\n' +
              '      "ers"\n' +
              '    ]\n' +
              '  },\n' +
              '  "statusCode": 420,\n' +
              '  "body": {\n' +
              '    "nope": "lol"\n' +
              '  }\n' +
              '}'
            : 'i had a vuln but i eated it lol',
          ],
        ])
        t.strictSame(LOGS, [['audit', 'hello, this didnt work']])
        t.end()
      })
    })
  }
  t.end()
})

t.test('completion', t => {
  const Audit = require('../../lib/audit.js')
  const audit = new Audit({})
  t.test('fix', async t => {
    t.resolveMatch(audit.completion({ conf: { argv: { remain: ['npm', 'audit'] } } }), ['fix'], 'completes to fix')
    t.end()
  })

  t.test('subcommand fix', t => {
    t.resolveMatch(audit.completion({ conf: { argv: { remain: ['npm', 'audit', 'fix'] } } }), [], 'resolves to ?')
    t.end()
  })

  t.test('subcommand not recognized', t => {
    t.rejects(
      audit.completion({ conf: { argv: { remain: ['npm', 'audit', 'repare'] } } }),
      { message: 'repare not recognized' }
    )
    t.end()
  })

  t.end()
})
