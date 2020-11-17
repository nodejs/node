const { test } = require('tap')
const requireInject = require('require-inject')

test('pings', async (t) => {
  t.plan(3)

  const options = { fake: 'options' }
  const response = { some: 'details' }
  const ping = requireInject('../../../lib/utils/ping.js', {
    'npm-registry-fetch': (url, opts) => {
      t.equal(url, '/-/ping?write=true', 'calls the correct url')
      t.equal(opts, options, 'passes through options')
      return { json: () => Promise.resolve(response) }
    },
  })

  const res = await ping(options)
  t.match(res, response, 'returns json response')
})

test('catches errors and returns empty json', async (t) => {
  t.plan(3)

  const options = { fake: 'options' }
  const response = { some: 'details' }
  const ping = requireInject('../../../lib/utils/ping.js', {
    'npm-registry-fetch': (url, opts) => {
      t.equal(url, '/-/ping?write=true', 'calls the correct url')
      t.equal(opts, options, 'passes through options')
      return { json: () => Promise.reject(response) }
    },
  })

  const res = await ping(options)
  t.match(res, {}, 'returns empty json response')
})
