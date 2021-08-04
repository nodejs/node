const pacote = require('pacote')

const formatDiff = require('./lib/format-diff.js')
const getTarball = require('./lib/tarball.js')
const untar = require('./lib/untar.js')

const argsError = () =>
  Object.assign(
    new TypeError('libnpmdiff needs two arguments to compare'),
    { code: 'EDIFFARGS' }
  )
const diff = async (specs, opts = {}) => {
  if (specs.length !== 2)
    throw argsError()

  const [
    aManifest,
    bManifest,
  ] =
    await Promise.all(specs.map(spec => pacote.manifest(spec, opts)))

  const versions = {
    a: aManifest.version,
    b: bManifest.version,
  }

  // fetches tarball using pacote
  const [a, b] = await Promise.all([
    getTarball(aManifest, opts),
    getTarball(bManifest, opts),
  ])

  // read all files
  // populates `files` and `refs`
  const {
    files,
    refs,
  } = await untar([
    {
      prefix: 'a/',
      item: a,
    },
    {
      prefix: 'b/',
      item: b,
    },
  ], opts)

  return formatDiff({
    files,
    opts,
    refs,
    versions,
  })
}

module.exports = diff
