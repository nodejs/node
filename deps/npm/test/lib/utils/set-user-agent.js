const t = require('tap')
const requireInject = require('require-inject')

let ci = null
const ciDetect = () => ci

const setUserAgent = requireInject('../../../lib/utils/set-user-agent.js', {
  '@npmcli/ci-detect': ciDetect
})

const defaultUA = 'npm/{npm-version} node/{node-version} {platform} {arch} {ci}'

const config = {
  settings: {
    'node-version': '123.420.69',
    'user-agent': defaultUA
  },
  set: (k, v) => {
    if (k !== 'user-agent') {
      throw new Error('setting the wrong thing')
    }
    config.settings['user-agent'] = v
  },
  get: (k) => {
    if (k !== 'user-agent' && k !== 'node-version') {
      throw new Error('setting the wrong thing')
    }
    return config.settings[k]
  }
}

t.test('not in CI, set user agent', t => {
  setUserAgent(config)
  t.equal(config.get('user-agent'), `npm/${require('../../../package.json').version} node/123.420.69 ${process.platform} ${process.arch}`)
  config.set('user-agent', defaultUA)
  t.end()
})

t.test('in CI, set user agent', t => {
  ci = 'something'
  setUserAgent(config)
  t.equal(config.get('user-agent'), `npm/${require('../../../package.json').version} node/123.420.69 ${process.platform} ${process.arch} ci/something`)
  config.set('user-agent', defaultUA)
  t.end()
})

t.test('no UA set, leave it blank', t => {
  ci = 'something'
  config.set('user-agent', null)
  setUserAgent(config)
  t.equal(config.get('user-agent'), '')
  config.set('user-agent', defaultUA)
  t.end()
})
