// use the defined flattening function, and copy over any scoped
// registries and registry-specific "nerfdart" configs verbatim
//
// TODO: make these getters so that we only have to make dirty
// the thing that changed, and then flatten the fields that
// could have changed when a config.set is called.
//
// TODO: move nerfdart auth stuff into a nested object that
// is only passed along to paths that end up calling npm-registry-fetch.
const definitions = require('./definitions.js')
const flatten = (obj, flat = {}) => {
  for (const [key, val] of Object.entries(obj)) {
    const def = definitions[key]
    if (def && def.flatten)
      def.flatten(key, obj, flat)
    else if (/@.*:registry$/i.test(key) || /^\/\//.test(key))
      flat[key] = val
  }

  // XXX make this the bin/npm-cli.js file explicitly instead
  // otherwise using npm programmatically is a bit of a pain.
  flat.npmBin = require.main ? require.main.filename
    : /* istanbul ignore next - not configurable property */ undefined
  flat.nodeBin = process.env.NODE || process.execPath

  // XXX should this be sha512?  is it even relevant?
  flat.hashAlgorithm = 'sha1'

  return flat
}

module.exports = flatten
