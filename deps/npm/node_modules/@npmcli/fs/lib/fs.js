const fs = require('fs')
const promisify = require('@gar/promisify')

const isLower = (s) => s === s.toLowerCase() && s !== s.toUpperCase()

const fsSync = Object.fromEntries(Object.entries(fs).filter(([k, v]) =>
  typeof v === 'function' && (k.endsWith('Sync') || !isLower(k[0]))
))

// this module returns the core fs async fns wrapped in a proxy that promisifies
// method calls within the getter. we keep it in a separate module so that the
// overridden methods have a consistent way to get to promisified fs methods
// without creating a circular dependency. the ctors and sync methods are kept untouched
module.exports = { ...promisify(fs), ...fsSync }
