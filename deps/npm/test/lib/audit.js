const t = require('tap')
const requireInject = require('require-inject')
const audit = require('../../lib/audit.js')

t.test('should audit using Arborist', t => {
  let ARB_ARGS = null
  let AUDIT_CALLED = false
  let REIFY_FINISH_CALLED = false
  let AUDIT_REPORT_CALLED = false
  let OUTPUT_CALLED = false
  let ARB_OBJ = null

  const audit = requireInject('../../lib/audit.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        json: false,
      },
    },
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
    '../../lib/utils/reify-finish.js': arb => {
      if (arb !== ARB_OBJ)
        throw new Error('got wrong object passed to reify-output')

      REIFY_FINISH_CALLED = true
    },
    '../../lib/utils/output.js': () => {
      OUTPUT_CALLED = true
    },
  })

  t.test('audit', t => {
    audit([], () => {
      t.match(ARB_ARGS, { audit: true, path: 'foo' })
      t.equal(AUDIT_CALLED, true, 'called audit')
      t.equal(AUDIT_REPORT_CALLED, true, 'called audit report')
      t.equal(OUTPUT_CALLED, true, 'called audit report')
      t.end()
    })
  })

  t.test('audit fix', t => {
    audit(['fix'], () => {
      t.equal(REIFY_FINISH_CALLED, true, 'called reify output')
      t.end()
    })
  })

  t.end()
})

t.test('should audit - json', t => {
  const audit = requireInject('../../lib/audit.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        json: true,
      },
    },
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
    '../../lib/utils/output.js': () => {},
  })

  audit([], (err) => {
    t.notOk(err, 'no errors')
    t.end()
  })
})

t.test('report endpoint error', t => {
  for (const json of [true, false]) {
    t.test(`json=${json}`, t => {
      const OUTPUT = []
      const LOGS = []
      const mocks = {
        '../../lib/npm.js': {
          prefix: 'foo',
          command: 'audit',
          flatOptions: {
            json,
          },
          log: {
            warn: (...warning) => LOGS.push(warning),
          },
        },
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
        '../../lib/utils/output.js': (...msg) => {
          OUTPUT.push(msg)
        },
      }
      // have to pass mocks to both to get the npm and output set right
      const auditError = requireInject('../../lib/utils/audit-error.js', mocks)
      const audit = requireInject('../../lib/audit.js', {
        ...mocks,
        '../../lib/utils/audit-error.js': auditError,
      })

      audit([], (err) => {
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
  t.test('fix', t => {
    audit.completion({
      conf: { argv: { remain: ['npm', 'audit'] } },
    }, (err, res) => {
      if (err)
        throw err
      const subcmd = res.pop()
      t.equals('fix', subcmd, 'completes to fix')
      t.end()
    })
  })

  t.test('subcommand fix', t => {
    audit.completion({
      conf: { argv: { remain: ['npm', 'audit', 'fix'] } },
    }, (err) => {
      if (err)
        throw err
      t.end()
    })
  })

  t.test('subcommand not recognized', t => {
    audit.completion({
      conf: { argv: { remain: ['npm', 'audit', 'repare'] } },
    }, (err) => {
      t.ok(err, 'not recognized')
      t.end()
    })
  })

  t.end()
})
