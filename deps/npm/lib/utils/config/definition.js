// class that describes a config key we know about
// this keeps us from defining a config key and not
// providing a default, description, etc.
//
// TODO: some kind of categorization system, so we can
// say "these are for registry access", "these are for
// version resolution" etc.

const required = [
  'type',
  'description',
  'default',
  'key',
]

const allowed = [
  'default',
  'defaultDescription',
  'deprecated',
  'description',
  'flatten',
  'hint',
  'key',
  'short',
  'type',
  'typeDescription',
  'usage',
  'envExport',
]

const {
  typeDefs: {
    semver: { type: semver },
    Umask: { type: Umask },
    url: { type: url },
    path: { type: path },
  },
} = require('@npmcli/config')

class Definition {
  constructor (key, def) {
    this.key = key
    // if it's set falsey, don't export it, otherwise we do by default
    this.envExport = true
    Object.assign(this, def)
    this.validate()
    if (!this.defaultDescription)
      this.defaultDescription = describeValue(this.default)
    if (!this.typeDescription)
      this.typeDescription = describeType(this.type)
    // hint is only used for non-boolean values
    if (!this.hint) {
      if (this.type === Number)
        this.hint = '<number>'
      else
        this.hint = `<${this.key}>`
    }
    if (!this.usage)
      this.usage = describeUsage(this)
  }

  validate () {
    for (const req of required) {
      if (!Object.prototype.hasOwnProperty.call(this, req))
        throw new Error(`config lacks ${req}: ${this.key}`)
    }
    if (!this.key)
      throw new Error(`config lacks key: ${this.key}`)
    for (const field of Object.keys(this)) {
      if (!allowed.includes(field))
        throw new Error(`config defines unknown field ${field}: ${this.key}`)
    }
  }

  // a textual description of this config, suitable for help output
  describe () {
    const description = unindent(this.description)
    const noEnvExport = this.envExport ? '' : `
This value is not exported to the environment for child processes.
`
    const deprecated = !this.deprecated ? ''
      : `* DEPRECATED: ${unindent(this.deprecated)}\n`
    return wrapAll(`#### \`${this.key}\`

* Default: ${unindent(this.defaultDescription)}
* Type: ${unindent(this.typeDescription)}
${deprecated}
${description}
${noEnvExport}`)
  }
}

const describeUsage = (def) => {
  let key = `--${def.key}`
  if (def.short && typeof def.short === 'string')
    key = `-${def.short}|${key}`

  // Single type
  if (!Array.isArray(def.type))
    return `${key}${def.type === Boolean ? '' : ' ' + def.hint}`

  // Multiple types
  let types = def.type
  const multiple = types.includes(Array)
  const bool = types.includes(Boolean)

  // null type means optional and doesn't currently affect usage output since
  // all non-optional params have defaults so we render everything as optional
  types = types.filter(t => t !== null && t !== Array && t !== Boolean)

  if (!types.length)
    return key

  let description
  if (!types.some(t => typeof t !== 'string'))
    // Specific values, use specifics given
    description = `<${types.filter(d => d).join('|')}>`
  else {
    // Generic values, use hint
    description = def.hint
  }

  if (bool)
    key = `${key}|${key}`

  const usage = `${key} ${description}`
  if (multiple)
    return `${usage} [${usage} ...]`
  else
    return usage
}

const describeType = type => {
  if (Array.isArray(type)) {
    const descriptions = type
      .filter(t => t !== Array)
      .map(t => describeType(t))

    // [a] => "a"
    // [a, b] => "a or b"
    // [a, b, c] => "a, b, or c"
    // [a, Array] => "a (can be set multiple times)"
    // [a, Array, b] => "a or b (can be set multiple times)"
    const last = descriptions.length > 1 ? [descriptions.pop()] : []
    const oxford = descriptions.length > 1 ? ', or ' : ' or '
    const words = [descriptions.join(', ')].concat(last).join(oxford)
    const multiple = type.includes(Array) ? ' (can be set multiple times)'
      : ''
    return `${words}${multiple}`
  }

  // Note: these are not quite the same as the description printed
  // when validation fails.  In that case, we want to give the user
  // a bit more information to help them figure out what's wrong.
  switch (type) {
    case String:
      return 'String'
    case Number:
      return 'Number'
    case Umask:
      return 'Octal numeric string in range 0000..0777 (0..511)'
    case Boolean:
      return 'Boolean'
    case Date:
      return 'Date'
    case path:
      return 'Path'
    case semver:
      return 'SemVer string'
    case url:
      return 'URL'
    default:
      return describeValue(type)
  }
}

// if it's a string, quote it.  otherwise, just cast to string.
const describeValue = val =>
  typeof val === 'string' ? JSON.stringify(val) : String(val)

const unindent = s => {
  // get the first \n followed by a bunch of spaces, and pluck off
  // that many spaces from the start of every line.
  const match = s.match(/\n +/)
  return !match ? s.trim() : s.split(match[0]).join('\n').trim()
}

const wrap = (s) => {
  const cols = Math.min(Math.max(20, process.stdout.columns) || 80, 80) - 5
  return unindent(s).split(/[ \n]+/).reduce((left, right) => {
    const last = left.split('\n').pop()
    const join = last.length && last.length + right.length > cols ? '\n' : ' '
    return left + join + right
  })
}

const wrapAll = s => {
  let inCodeBlock = false
  return s.split('\n\n').map(block => {
    if (inCodeBlock || block.startsWith('```')) {
      inCodeBlock = !block.endsWith('```')
      return block
    }

    if (block.charAt(0) === '*') {
      return '* ' + block.substr(1).trim().split('\n* ').map(li => {
        return wrap(li).replace(/\n/g, '\n  ')
      }).join('\n* ')
    } else
      return wrap(block)
  }).join('\n\n')
}

module.exports = Definition
