const t = require('tap')
const tmock = require('../../fixtures/tmock')

const webAuthCheckLogin = async () => {
  return { token: 'otp-token' }
}

const webauth = tmock(t, '{LIB}/utils/web-auth.js', {
  'npm-profile': { webAuthCheckLogin },
})

const initialUrl = 'https://example.com/auth'
const doneUrl = 'https://example.com/done'
const opts = {}

t.test('returns token on success', async (t) => {
  const opener = async () => {}
  const result = await webauth(opener, initialUrl, doneUrl, opts)
  t.equal(result, 'otp-token')
})

t.test('closes opener when auth check finishes', async (t) => {
  const opener = (_url, emitter) => {
    return new Promise((resolve) => {
      // the only way to finish this promise is to emit abort on the emitter
      emitter.addListener('abort', () => {
        resolve()
      })
    })
  }
  const result = await webauth(opener, initialUrl, doneUrl, opts)
  t.equal(result, 'otp-token')
})
