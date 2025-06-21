const hasIntl = typeof Intl === 'object' && !!Intl
const Collator = hasIntl && Intl.Collator
const cache = new Map()

const collatorCompare = (locale, opts) => {
  const collator = new Collator(locale, opts)
  return (a, b) => collator.compare(a, b)
}

const localeCompare = (locale, opts) => (a, b) => a.localeCompare(b, locale, opts)

const knownOptions = [
  'sensitivity',
  'numeric',
  'ignorePunctuation',
  'caseFirst',
]

const { hasOwnProperty } = Object.prototype

module.exports = (locale, options = {}) => {
  if (!locale || typeof locale !== 'string')
    throw new TypeError('locale required')

  const opts = knownOptions.reduce((opts, k) => {
    if (hasOwnProperty.call(options, k)) {
      opts[k] = options[k]
    }
    return opts
  }, {})
  const key = `${locale}\n${JSON.stringify(opts)}`

  if (cache.has(key))
    return cache.get(key)

  const compare = hasIntl
    ? collatorCompare(locale, opts)
    : localeCompare(locale, opts)
  cache.set(key, compare)

  return compare
}
