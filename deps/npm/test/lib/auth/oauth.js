const t = require('tap')

t.test('oauth login', (t) => {
  t.plan(3)
  const oauthOpts = {
    creds: {},
    registry: 'https://diff-registry.npmjs.org/',
    scope: 'myscope',
  }

  const npm = {
    config: {
      set: (key, value) => {
        t.equal(key, 'sso-type', 'should define sso-type')
        t.equal(value, 'oauth', 'should set sso-type to oauth')
      },
    },
  }
  const oauth = t.mock('../../../lib/auth/oauth.js', {
    '../../../lib/auth/sso.js': (npm, opts) => {
      t.equal(opts, oauthOpts, 'should forward opts')
    },
  })

  oauth(npm, oauthOpts)

  t.end()
})
