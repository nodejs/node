const t = require('tap')

const LOGS = []
const OUTPUT = []
const output = (...msg) => OUTPUT.push(msg)
const auditError = require('../../../lib/utils/audit-error.js')

const npm = {
  command: null,
  flatOptions: {},
  log: {
    warn: (...msg) => LOGS.push(msg),
  },
  output,
}
t.afterEach(() => {
  npm.flatOptions = {}
  OUTPUT.length = 0
  LOGS.length = 0
})

t.test('no error, not audit command', t => {
  npm.command = 'install'
  t.equal(auditError(npm, {}), false, 'no error')
  t.strictSame(OUTPUT, [], 'no output')
  t.strictSame(LOGS, [], 'no warnings')
  t.end()
})

t.test('error, not audit command', t => {
  npm.command = 'install'
  t.equal(auditError(npm, {
    error: {
      message: 'message',
      body: Buffer.from('body'),
      method: 'POST',
      uri: 'https://example.com/not/a/registry',
      headers: {
        head: ['ers'],
      },
      statusCode: '420',
    },
  }), true, 'had error')
  t.strictSame(OUTPUT, [], 'no output')
  t.strictSame(LOGS, [], 'no warnings')
  t.end()
})

t.test('error, audit command, not json', t => {
  npm.command = 'audit'
  npm.flatOptions.json = false
  t.throws(() => auditError(npm, {
    error: {
      message: 'message',
      body: Buffer.from('body'),
      method: 'POST',
      uri: 'https://example.com/not/a/registry',
      headers: {
        head: ['ers'],
      },
      statusCode: '420',
    },
  }))

  t.strictSame(OUTPUT, [['body']], 'some output')
  t.strictSame(LOGS, [['audit', 'message']], 'some warnings')
  t.end()
})

t.test('error, audit command, json', t => {
  npm.command = 'audit'
  npm.flatOptions.json = true
  t.throws(() => auditError(npm, {
    error: {
      message: 'message',
      body: { response: 'body' },
      method: 'POST',
      uri: 'https://example.com/not/a/registry',
      headers: {
        head: ['ers'],
      },
      statusCode: '420',
    },
  }))

  t.strictSame(OUTPUT, [
    [
      '{\n' +
        '  "message": "message",\n' +
        '  "method": "POST",\n' +
        '  "uri": "https://example.com/not/a/registry",\n' +
        '  "headers": {\n' +
        '    "head": [\n' +
        '      "ers"\n' +
        '    ]\n' +
        '  },\n' +
        '  "statusCode": "420",\n' +
        '  "body": {\n' +
        '    "response": "body"\n' +
        '  }\n' +
        '}',
    ],
  ], 'some output')
  t.strictSame(LOGS, [['audit', 'message']], 'some warnings')
  t.end()
})
