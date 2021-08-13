/**
 * @typedef Option
 * @property {'boolean'|'string'} [type='string']
 * @property {string} long
 * @property {string} description
 * @property {string} [value]
 * @property {string} [short]
 * @property {boolean|string} [default='']
 * @property {boolean} [truelike=false]
 */

/** @type {Option[]} */
export const schema = [
  {
    long: 'help',
    description: 'output usage information',
    short: 'h',
    type: 'boolean',
    default: false
  },
  {
    long: 'version',
    description: 'output version number',
    short: 'v',
    type: 'boolean',
    default: false
  },
  {
    long: 'output',
    description: 'specify output location',
    short: 'o',
    value: '[path]'
  },
  {
    long: 'rc-path',
    description: 'specify configuration file',
    short: 'r',
    type: 'string',
    value: '<path>'
  },
  {
    long: 'ignore-path',
    description: 'specify ignore file',
    short: 'i',
    type: 'string',
    value: '<path>'
  },
  {
    long: 'setting',
    description: 'specify settings',
    short: 's',
    type: 'string',
    value: '<settings>'
  },
  {
    long: 'ext',
    description: 'specify extensions',
    short: 'e',
    type: 'string',
    value: '<extensions>'
  },
  {
    long: 'use',
    description: 'use plugins',
    short: 'u',
    type: 'string',
    value: '<plugins>'
  },
  {
    long: 'watch',
    description: 'watch for changes and reprocess',
    short: 'w',
    type: 'boolean',
    default: false
  },
  {
    long: 'quiet',
    description: 'output only warnings and errors',
    short: 'q',
    type: 'boolean',
    default: false
  },
  {
    long: 'silent',
    description: 'output only errors',
    short: 'S',
    type: 'boolean',
    default: false
  },
  {
    long: 'frail',
    description: 'exit with 1 on warnings',
    short: 'f',
    type: 'boolean',
    default: false
  },
  {
    long: 'tree',
    description: 'specify input and output as syntax tree',
    short: 't',
    type: 'boolean',
    default: false
  },
  {
    long: 'report',
    description: 'specify reporter',
    type: 'string',
    value: '<reporter>'
  },
  {
    long: 'file-path',
    description: 'specify path to process as',
    type: 'string',
    value: '<path>'
  },
  {
    long: 'ignore-path-resolve-from',
    description: 'resolve patterns in `ignore-path` from its directory or cwd',
    type: 'string',
    value: 'dir|cwd',
    default: 'dir'
  },
  {
    long: 'ignore-pattern',
    description: 'specify ignore patterns',
    type: 'string',
    value: '<globs>'
  },
  {
    long: 'silently-ignore',
    description: 'do not fail when given ignored files',
    type: 'boolean'
  },
  {
    long: 'tree-in',
    description: 'specify input as syntax tree',
    type: 'boolean'
  },
  {
    long: 'tree-out',
    description: 'output syntax tree',
    type: 'boolean'
  },
  {
    long: 'inspect',
    description: 'output formatted syntax tree',
    type: 'boolean'
  },
  {
    long: 'stdout',
    description: 'specify writing to stdout',
    type: 'boolean',
    truelike: true
  },
  {
    long: 'color',
    description: 'specify color in report',
    type: 'boolean',
    default: true
  },
  {
    long: 'config',
    description: 'search for configuration files',
    type: 'boolean',
    default: true
  },
  {
    long: 'ignore',
    description: 'search for ignore files',
    type: 'boolean',
    default: true
  }
]
