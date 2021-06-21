const { resolve } = require('path')
const t = require('tap')
const pacote = require('pacote')
const untar = require('../lib/untar.js')

t.test('untar simple package', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/simple-output-2.2.1.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v}`).join('\n'),
    'should return map of filenames to its contents'
  )
  t.matchSnapshot(refs.get('a/LICENSE').content, 'should have read contents')
})

t.test('untar package with folders', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/archive.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v}`).join('\n'),
    'should return map of filenames to its contents'
  )
  t.matchSnapshot(
    refs.get('a/lib/utils/b.js').content,
    'should have read contents'
  )
})

t.test('filter files', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/simple-output-2.2.1.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      './LICENSE',
      'missing-file',
      'README.md',
    ],
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v.content}`).join('\n'),
    'should return map of filenames with valid contents'
  )
})

t.test('filter files using glob expressions', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/archive.tgz'))
  const cwd = t.testdir({
    lib: {
      'index.js': '',
      utils: {
        '/b.js': '',
      },
    },
    'package-lock.json': '',
    'package.json': '',
    test: {
      '/index.js': '',
      utils: {
        'b.js': '',
      },
    },
  })

  const _cwd = process.cwd()
  process.chdir(cwd)
  t.teardown(() => {
    process.chdir(_cwd)
  })

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      './lib/**',
      '*-lock.json',
      'test\\*', // windows-style sep should be normalized
    ],
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v.content}`).join('\n'),
    'should return map of filenames with valid contents'
  )
})

t.test('match files by end of filename', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/archive.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      '*.js',
    ],
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v.content}`).join('\n'),
    'should return map of filenames with valid contents'
  )
})

t.test('filter files by exact filename', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/archive.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      'index.js',
    ],
  })

  t.matchSnapshot([...files].join('\n'), 'should return no filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v.content}`).join('\n'),
    'should return no filenames'
  )
})

t.test('match files by simple folder name', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/archive.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      'lib',
    ],
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v.content}`).join('\n'),
    'should return map of filenames with valid contents'
  )
})

t.test('match files by simple folder name variation', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/archive.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      './test/',
    ],
  })

  t.matchSnapshot([...files].join('\n'), 'should return list of filenames')
  t.matchSnapshot(
    [...refs.entries()].map(([k, v]) => `${k}: ${!!v.content}`).join('\n'),
    'should return map of filenames with valid contents'
  )
})

t.test('filter out all files', async t => {
  const item =
    await pacote.tarball(resolve('./test/fixtures/simple-output-2.2.1.tgz'))

  const {
    files,
    refs,
  } = await untar({
    item,
    prefix: 'a/',
  }, {
    diffFiles: [
      'non-matching-pattern',
    ],
  })

  t.equal(files.size, 0, 'should have no files')
  t.equal(refs.size, 0, 'should have no refs')
})
