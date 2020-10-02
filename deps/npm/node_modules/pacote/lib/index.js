const { get } = require('./fetcher.js')
module.exports = {
  resolve: (spec, opts) => get(spec, opts).resolve(),
  extract: (spec, dest, opts) => get(spec, opts).extract(dest),
  manifest: (spec, opts) => get(spec, opts).manifest(),
  tarball: (spec, opts) => get(spec, opts).tarball(),
  packument: (spec, opts) => get(spec, opts).packument(),
}
module.exports.tarball.stream = (spec, handler, opts) =>
  get(spec, opts).tarballStream(handler)
module.exports.tarball.file = (spec, dest, opts) =>
  get(spec, opts).tarballFile(dest)
