/**
 * Get the total count of `character` in `value`.
 *
 * @param {any} value Content, coerced to string
 * @param {string} character Single character to look for
 * @return {number} Number of times `character` occurred in `value`.
 */
export function ccount(value, character) {
  var source = String(value)
  var count = 0
  var index

  if (typeof character !== 'string') {
    throw new Error('Expected character')
  }

  index = source.indexOf(character)

  while (index !== -1) {
    count++
    index = source.indexOf(character, index + character.length)
  }

  return count
}
