const { test } = require('tap')
const requireInject = require('require-inject')
const audit = require('../../lib/audit.js')

test('should audit using Arborist', t => {
  let ARB_ARGS = null
  let AUDIT_CALLED = false
  let REIFY_OUTPUT_CALLED = false
  let AUDIT_REPORT_CALLED = false
  let OUTPUT_CALLED = false
  let ARB_OBJ = null

  const audit = requireInject('../../lib/audit.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        json: false
      },
    },
    'npm-audit-report': () => {
      AUDIT_REPORT_CALLED = true
      return {
        report: 'there are vulnerabilities',
        exitCode: 0
      }
    },
    '@npmcli/arborist': function (args) {
      ARB_ARGS = args
      ARB_OBJ = this
      this.audit = () => {
        AUDIT_CALLED = true
      }
    },
    '../../lib/utils/reify-output.js': arb => {
      if (arb !== ARB_OBJ) {
        throw new Error('got wrong object passed to reify-output')
      }
      REIFY_OUTPUT_CALLED = true
    },
    '../../lib/utils/output.js': () => {
      OUTPUT_CALLED = true
    }
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
      t.equal(REIFY_OUTPUT_CALLED, true, 'called reify output')
      t.end()
    })
  })

  t.end()
})

test('should audit - json', t => {
  const audit = requireInject('../../lib/audit.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        json: true
      },
    },
    'npm-audit-report': () => ({
      report: 'there are vulnerabilities',
      exitCode: 0
    }),
    '@npmcli/arborist': function () {
      this.audit = () => {}
    },
    '../../lib/utils/reify-output.js': () => {},
    '../../lib/utils/output.js': () => {}
  })

  audit([], (err) => {
    t.notOk(err, 'no errors')
    t.end()
  })
})

test('completion', t => {
  t.test('fix', t => {
    audit.completion({
      conf: { argv: { remain: ['npm', 'audit'] } }
    }, (err, res) => {
      const subcmd = res.pop()
      t.equals('fix', subcmd, 'completes to fix')
      t.end()
    })
  })

  t.test('subcommand fix', t => {
    audit.completion({
      conf: { argv: { remain: ['npm', 'audit', 'fix'] } }
    }, (err) => {
      t.notOk(err, 'no errors')
      t.end()
    })
  })

  t.test('subcommand not recognized', t => {
    audit.completion({
      conf: { argv: { remain: ['npm', 'audit', 'repare'] } }
    }, (err) => {
      t.ok(err, 'not recognized')
      t.end()
    })
  })

  t.end()
})