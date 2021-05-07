const t = require('tap')

t.test('saml login', (t) => {
  t.plan(3)
  const samlOpts = {
    creds: {},
    registry: 'https://diff-registry.npmjs.org/',
    scope: 'myscope',
  }

  const npm = {
    config: {
      set: (key, value) => {
        t.equal(key, 'sso-type', 'should define sso-type')
        t.equal(value, 'saml', 'should set sso-type to saml')
      },
    },
  }
  const saml = t.mock('../../../lib/auth/saml.js', {
    '../../../lib/auth/sso.js': (npm, opts) => {
      t.equal(opts, samlOpts, 'should forward opts')
    },
  })

  saml(npm, samlOpts)

  t.end()
})
