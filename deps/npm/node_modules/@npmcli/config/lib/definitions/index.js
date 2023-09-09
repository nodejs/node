const definitions = require('./definitions.js')

// use the defined flattening function, and copy over any scoped
// registries and registry-specific "nerfdart" configs verbatim
//
// TODO: make these getters so that we only have to make dirty
// the thing that changed, and then flatten the fields that
// could have changed when a config.set is called.
//
// TODO: move nerfdart auth stuff into a nested object that
// is only passed along to paths that end up calling npm-registry-fetch.
const flatten = (obj, flat = {}) => {
  for (const [key, val] of Object.entries(obj)) {
    const def = definitions[key]
    if (def && def.flatten) {
      def.flatten(key, obj, flat)
    } else if (/@.*:registry$/i.test(key) || /^\/\//.test(key)) {
      flat[key] = val
    }
  }
  return flat
}

const definitionProps = Object.entries(definitions)
  .reduce((acc, [key, { short = [], default: d }]) => {
  // can be either an array or string
    for (const s of [].concat(short)) {
      acc.shorthands[s] = [`--${key}`]
    }
    acc.defaults[key] = d
    return acc
  }, { shorthands: {}, defaults: {} })

// aliases where they get expanded into a completely different thing
// these are NOT supported in the environment or npmrc files, only
// expanded on the CLI.
// TODO: when we switch off of nopt, use an arg parser that supports
// more reasonable aliasing and short opts right in the definitions set.
const shorthands = {
  'enjoy-by': ['--before'],
  d: ['--loglevel', 'info'],
  dd: ['--loglevel', 'verbose'],
  ddd: ['--loglevel', 'silly'],
  quiet: ['--loglevel', 'warn'],
  q: ['--loglevel', 'warn'],
  s: ['--loglevel', 'silent'],
  silent: ['--loglevel', 'silent'],
  verbose: ['--loglevel', 'verbose'],
  desc: ['--description'],
  help: ['--usage'],
  local: ['--no-global'],
  n: ['--no-yes'],
  no: ['--no-yes'],
  porcelain: ['--parseable'],
  readonly: ['--read-only'],
  reg: ['--registry'],
  iwr: ['--include-workspace-root'],
  ...definitionProps.shorthands,
}

module.exports = {
  defaults: definitionProps.defaults,
  definitions,
  flatten,
  shorthands,
}
