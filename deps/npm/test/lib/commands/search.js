const t = require('tap')
const Minipass = require('minipass')
const { fake: mockNpm } = require('../../fixtures/mock-npm')
const libnpmsearchResultFixture =
  require('../../fixtures/libnpmsearch-stream-result.js')

let result = ''
const flatOptions = {
  search: {
    exclude: null,
    limit: 20,
    opts: '',
  },
}
const config = {
  json: false,
  parseable: false,
}
const npm = mockNpm({
  config,
  flatOptions: { ...flatOptions },
  output: (...msg) => {
    result += msg.join('\n')
  },
})
const npmlog = {
  silly () {},
  clearProgress () {},
}
const libnpmsearch = {
  stream () {},
}
const mocks = {
  npmlog,
  libnpmsearch,
  '../../../lib/utils/usage.js': () => 'usage instructions',
}

t.afterEach(() => {
  result = ''
  config.json = false
  config.parseable = false
  npm.flatOptions = { ...flatOptions }
})

const Search = t.mock('../../../lib/commands/search.js', mocks)
const search = new Search(npm)

t.test('no args', async t => {
  await t.rejects(
    search.exec([]),
    /search must be called with arguments/,
    'should throw usage instructions'
  )
})

t.test('search <name>', async t => {
  const src = new Minipass()
  src.objectMode = true
  const libnpmsearch = {
    stream () {
      return src
    },
  }

  const Search = t.mock('../../../lib/commands/search.js', {
    ...mocks,
    libnpmsearch,
  })
  const search = new Search(npm)

  for (const i of libnpmsearchResultFixture) {
    src.write(i)
  }

  src.end()

  await search.exec(['libnpm'])
  t.matchSnapshot(result, 'should have expected search results')
})

t.test('search <name> --json', async t => {
  const src = new Minipass()
  src.objectMode = true

  npm.flatOptions.json = true
  config.json = true
  const libnpmsearch = {
    stream () {
      return src
    },
  }

  const Search = t.mock('../../../lib/commands/search.js', {
    ...mocks,
    libnpmsearch,
  })
  const search = new Search(npm)

  for (const i of libnpmsearchResultFixture) {
    src.write(i)
  }

  src.end()
  await search.exec(['libnpm'])

  const parsedResult = JSON.parse(result)
  parsedResult.forEach((entry) => {
    entry.date = new Date(entry.date)
  })

  t.same(
    parsedResult,
    libnpmsearchResultFixture,
    'should have expected search results as json'
  )

  config.json = false
})

t.test('search <invalid-module> --json', async t => {
  const src = new Minipass()
  src.objectMode = true

  npm.flatOptions.json = true
  config.json = true
  const libnpmsearch = {
    stream () {
      return src
    },
  }

  const Search = t.mock('../../../lib/commands/search.js', {
    ...mocks,
    libnpmsearch,
  })
  const search = new Search(npm)

  src.end()
  await search.exec(['foo'])

  t.equal(result, '\n[]\n', 'should have expected empty square brackets')

  config.json = false
})

t.test('search <name> --searchexclude --searchopts', async t => {
  npm.flatOptions.search = {
    ...flatOptions.search,
    exclude: '',
  }

  const src = new Minipass()
  src.objectMode = true
  const libnpmsearch = {
    stream () {
      return src
    },
  }

  const Search = t.mock('../../../lib/commands/search.js', {
    ...mocks,
    libnpmsearch,
  })
  const search = new Search(npm)

  src.write({
    name: 'foo',
    scope: 'unscoped',
    version: '1.0.0',
    description: '',
    keywords: [],
    date: null,
    author: { name: 'Foo', email: 'foo@npmjs.com' },
    publisher: { name: 'Foo', email: 'foo@npmjs.com' },
    maintainers: [
      { username: 'foo', email: 'foo@npmjs.com' },
    ],
  })
  src.write({
    name: 'libnpmversion',
    scope: 'unscoped',
    version: '1.0.0',
    description: '',
    keywords: [],
    date: null,
    author: { name: 'Foo', email: 'foo@npmjs.com' },
    publisher: { name: 'Foo', email: 'foo@npmjs.com' },
    maintainers: [
      { username: 'foo', email: 'foo@npmjs.com' },
    ],
  })

  src.end()
  await search.exec(['foo'])

  t.matchSnapshot(result, 'should have filtered expected search results')
})

t.test('empty search results', async t => {
  const src = new Minipass()
  src.objectMode = true
  const libnpmsearch = {
    stream () {
      return src
    },
  }

  const Search = t.mock('../../../lib/commands/search.js', {
    ...mocks,
    libnpmsearch,
  })
  const search = new Search(npm)

  src.end()
  await search.exec(['foo'])

  t.matchSnapshot(result, 'should have expected search results')
})

t.test('search api response error', async t => {
  const src = new Minipass()
  src.objectMode = true
  const libnpmsearch = {
    stream () {
      return src
    },
  }

  const Search = t.mock('../../../lib/commands/search.js', {
    ...mocks,
    libnpmsearch,
  })
  const search = new Search(npm)

  setImmediate(() => {
    src.emit('error', new Error('ERR'))
    src.end()
  })

  await t.rejects(
    search.exec(['foo']),
    /ERR/,
    'should throw response error'
  )
})
