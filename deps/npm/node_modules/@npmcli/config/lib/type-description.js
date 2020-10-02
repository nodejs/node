// return the description of the valid values of a field
// returns a string for one thing, or an array of descriptions
const typeDefs = require('./type-defs.js')
const typeDescription = t => {
  if (!t || typeof t !== 'function' && typeof t !== 'object')
    return t

  if (Array.isArray(t))
    return t.map(t => typeDescription(t))

  for (const { type, description } of Object.values(typeDefs)) {
    if (type === t)
      return description || type
  }

  return t
}
module.exports = t => [].concat(typeDescription(t)).filter(t => t !== undefined)
