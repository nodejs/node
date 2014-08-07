"use strict"

var wcwidth = require('./width')

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
  var length = 1 + max - wcwidth(str)
  if (length <= 0) return str
  return str + Array.apply(null, {length: length})
  .join(chr || ' ')
}

/**
 * Pad `str` up to total length `max` with `chr`, on the left.
 * If `str` is longer than `max`, padRight will return `str` unaltered.
 *
 * @param String str string to pad
 * @param Number max total length of output string
 * @param String chr optional. Character to pad with. default: ' '
 * @return String padded str
 */

function padLeft(str, max, chr) {
  str = str != null ? str : ''
  str = String(str)
  var length = 1 + max - wcwidth(str)
  if (length <= 0) return str
  return Array.apply(null, {length: length}).join(chr || ' ') + str
}

/**
 * Split a String `str` into lines of maxiumum length `max`.
 * Splits on word boundaries. Preserves existing new lines.
 *
 * @param String str string to split
 * @param Number max length of each line
 * @return Array Array containing lines.
 */

function splitIntoLines(str, max) {
  function _splitIntoLines(str, max) {
    return str.trim().split(' ').reduce(function(lines, word) {
      var line = lines[lines.length - 1]
      if (line && wcwidth(line.join(' ')) + wcwidth(word) < max) {
        lines[lines.length - 1].push(word) // add to line
      }
      else lines.push([word]) // new line
      return lines
    }, []).map(function(l) {
      return l.join(' ')
    })
  }
  return str.split('\n').map(function(str) {
    return _splitIntoLines(str, max)
  }).reduce(function(lines, line) {
    return lines.concat(line)
  }, [])
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
  if (wcwidth(word) > max) {
    // slice is based on length no wcwidth
    var i = 0
    var wwidth = 0
    var limit = max - wcwidth(truncationChar)
    while (i < word.length) {
      var w = wcwidth(word.charAt(i))
      if(w + wwidth > limit)
        break
      wwidth += w
      ++i
    }

    var remainder = word.slice(i) // get remainder
    words.unshift(remainder) // save remainder for next loop

    word = word.slice(0, i) // grab truncated word
    word += truncationChar // add trailing … or whatever
  }
  result.push(word)
  return splitLongWords(words.join(' '), max, truncationChar, result)
}


/**
 * Truncate `str` into total width `max`
 * If `str` is shorter than `max`,  will return `str` unaltered.
 *
 * @param String str string to truncated
 * @param Number max total wcwidth of output string
 * @return String truncated str
 */

function truncateString(str, max) {

  str = str != null ? str : ''
  str = String(str)

  if(max == Infinity) return str

  var i = 0
  var wwidth = 0
  while (i < str.length) {
    var w = wcwidth(str.charAt(i))
    if(w + wwidth > max)
      break
    wwidth += w
    ++i
  }
  return str.slice(0, i)
}



/**
 * Exports
 */

module.exports.padRight = padRight
module.exports.padLeft = padLeft
module.exports.splitIntoLines = splitIntoLines
module.exports.splitLongWords = splitLongWords
module.exports.truncateString = truncateString

