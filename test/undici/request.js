const { test } = require('tap')
const { Request } = require('../lib/undici/request')
const { Readable } = require('stream')

test('Request with ReadableStream body requires duplex option', (t) => {
  t.plan(2)

  const stream = new Readable({
    read () {}
  })

  t.throws(() => {
    new Request('http://example.com', { body: stream })
  }, TypeError, 'should throw when duplex is missing')

  t.doesNotThrow(() => {
    new Request('http://example.com', { body: stream, duplex: 'auto' })
  }, 'should not throw when duplex is provided')
})