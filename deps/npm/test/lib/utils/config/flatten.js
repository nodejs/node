const t = require('tap')
const flatten = require('../../../../lib/utils/config/flatten.js')

require.main.filename = '/path/to/npm'
delete process.env.NODE
process.execPath = '/path/to/node'

const obj = {
  'save-exact': true,
  'save-prefix': 'ignored',
  'save-dev': true,
  '@foobar:registry': 'https://foo.bar.com/',
  '//foo.bar.com:_authToken': 'foobarbazquuxasdf',
  userconfig: '/path/to/.npmrc',
}

const flat = flatten(obj)
t.strictSame(flat, {
  saveType: 'dev',
  savePrefix: '',
  '@foobar:registry': 'https://foo.bar.com/',
  '//foo.bar.com:_authToken': 'foobarbazquuxasdf',
  npmBin: '/path/to/npm',
  nodeBin: '/path/to/node',
  hashAlgorithm: 'sha1',
})

// now flatten something else on top of it.
process.env.NODE = '/usr/local/bin/node.exe'
flatten({ 'save-dev': false }, flat)
t.strictSame(flat, {
  savePrefix: '',
  '@foobar:registry': 'https://foo.bar.com/',
  '//foo.bar.com:_authToken': 'foobarbazquuxasdf',
  npmBin: '/path/to/npm',
  nodeBin: '/usr/local/bin/node.exe',
  hashAlgorithm: 'sha1',
})
