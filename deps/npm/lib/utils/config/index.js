const flatten = require('./flatten.js')
const definitions = require('./definitions.js')
const describeAll = require('./describe-all.js')

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
}

for (const [key, { short }] of Object.entries(definitions)) {
  if (!short) {
    continue
  }
  // can be either an array or string
  for (const s of [].concat(short)) {
    shorthands[s] = [`--${key}`]
  }
}

module.exports = {
  get defaults () {
    // NB: 'default' is a reserved word
    return Object.entries(definitions).map(([key, { default: def }]) => {
      return [key, def]
    }).reduce((defaults, [key, def]) => {
      defaults[key] = def
      return defaults
    }, {})
  },
  definitions,
  flatten,
  shorthands,
  describeAll,
}
