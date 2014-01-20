/**
 * Pad `str` up to total length `max` with `chr`.
 * If `str` is longer than `max`, padRight will return `str` unaltered.
 *
 * @param String str string to pad
 * @param Number max total length of output string
 * @param String chr optional. Character to pad with. default: ' '
 * @return String padded str
 */

function padRight(str, max, chr) {
  str = str != null ? str : ''
  str = String(str)
  var length = 1 + max - str.length
  if (length <= 0) return str
  return str + Array.apply(null, {length: length})
  .join(chr || ' ')
}

/**
 * Split a String `str` into lines of maxiumum length `max`.
 * Splits on word boundaries.
 *
 * @param String str string to split
 * @param Number max length of each line
 * @return Array Array containing lines.
 */

function splitIntoLines(str, max) {
  return str.trim().split(' ').reduce(function(lines, word) {
    var line = lines[lines.length - 1]
    if (line && line.join(' ').length + word.length < max) {
      lines[lines.length - 1].push(word) // add to line
    }
    else lines.push([word]) // new line
    return lines
  }, []).map(function(l) {
    return l.join(' ')
  })
}

/**
 * Add spaces and `truncationChar` between words of
 * `str` which are longer than `max`.
 *
 * @param String str string to split
 * @param Number max length of each line
 * @param Number truncationChar character to append to split words
 * @return String
 */

function splitLongWords(str, max, truncationChar, result) {
  str = str.trim()
  result = result || []
  if (!str) return result.join(' ') || ''
  var words = str.split(' ')
  var word = words.shift() || str

  if (word.length > max) {
    var remainder = word.slice(max - truncationChar.length) // get remainder
    words.unshift(remainder) // save remainder for next loop

    word = word.slice(0, max - truncationChar.length) // grab truncated word
    word += truncationChar // add trailing … or whatever
  }
  result.push(word)
  return splitLongWords(words.join(' '), max, truncationChar, result)
}

/**
 * Exports
 */

module.exports.padRight = padRight
module.exports.splitIntoLines = splitIntoLines
module.exports.splitLongWords = splitLongWords
