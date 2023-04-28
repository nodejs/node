const t = require('tap')
const replaceInfo = require('../../../lib/utils/replace-info.js')

t.equal(
  replaceInfo(),
  undefined,
  'should return undefined item'
)

t.equal(
  replaceInfo(null),
  null,
  'should return null'
)

t.equal(
  replaceInfo(1234),
  1234,
  'should return numbers'
)

t.equal(
  replaceInfo('  ==  = = '),
  '  ==  = = ',
  'should return same string with only separators'
)

t.equal(
  replaceInfo(''),
  '',
  'should return empty string'
)

t.equal(
  replaceInfo('https://user:pass@registry.npmjs.org/'),
  'https://user:***@registry.npmjs.org/',
  'should replace single item'
)

t.equal(
  replaceInfo(`https://registry.npmjs.org/path/npm_${'a'.repeat('36')}`),
  'https://registry.npmjs.org/path/npm_***',
  'should replace single item token'
)

t.equal(
  replaceInfo('https://example.npmjs.org'),
  'https://example.npmjs.org',
  'should not replace single item with no password'
)

t.equal(
  replaceInfo('foo bar https://example.npmjs.org lorem ipsum'),
  'foo bar https://example.npmjs.org lorem ipsum',
  'should not replace single item with no password with multiple items'
)

t.equal(
  replaceInfo('https://user:pass@registry.npmjs.org/ http://a:b@reg.github.com'),
  'https://user:***@registry.npmjs.org/ http://a:***@reg.github.com',
  'should replace multiple items on a string'
)

t.equal(
  replaceInfo('Something https://user:pass@registry.npmjs.org/ foo bar'),
  'Something https://user:***@registry.npmjs.org/ foo bar',
  'should replace single item within a phrase'
)

t.equal(
  replaceInfo('Something --x=https://user:pass@registry.npmjs.org/ foo bar'),
  'Something --x=https://user:***@registry.npmjs.org/ foo bar',
  'should replace single item within a phrase separated by ='
)

t.same(
  replaceInfo([
    'Something https://user:pass@registry.npmjs.org/ foo bar',
    'http://foo:bar@registry.npmjs.org',
    'http://example.npmjs.org',
  ]),
  [
    'Something https://user:***@registry.npmjs.org/ foo bar',
    'http://foo:***@registry.npmjs.org',
    'http://example.npmjs.org',
  ],
  'should replace items in an array'
)

t.same(
  replaceInfo([
    'Something --x=https://user:pass@registry.npmjs.org/ foo bar',
    '--url=http://foo:bar@registry.npmjs.org',
    '--url=http://example.npmjs.org',
  ]),
  [
    'Something --x=https://user:***@registry.npmjs.org/ foo bar',
    '--url=http://foo:***@registry.npmjs.org',
    '--url=http://example.npmjs.org',
  ],
  'should replace items in an array with equals'
)

t.same(
  replaceInfo([
    'Something https://user:pass@registry.npmjs.org/ foo bar',
    null,
    [],
  ]),
  [
    'Something https://user:***@registry.npmjs.org/ foo bar',
    null,
    [],
  ],
  'should ignore invalid items of array'
)
