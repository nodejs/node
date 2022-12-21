'use strict'
module.exports = qw

function appendLast (arr, str) {
  var last = arr.length - 1
  if (last < 0) {
    arr.push(str)
  } else {
    var lastValue = String(arr[last])
    return arr[last] = lastValue + String(str)
  }
}

function qw () {
  if (!arguments[0].raw) throw new Error('qw is only usable as a tagged template literal')
  const args = Object.assign([], arguments[0])
  const values = [].slice.call(arguments, 1)
  const words = []
  let lastWordWasValue = false
  while (args.length) {
    const arg = args.shift()
    const concatValue = arg.length === 0 || arg.slice(-1) !== ' '
    if (arg.trim() !== '') {
      const theseWords = arg.replace(/^\s+|\s+$/g, '').replace(/\s+/g, ' ').split(/ /)
      if (lastWordWasValue && arg[0] !== ' ') {
        appendLast(words, theseWords.shift())
      }
      words.push.apply(words, theseWords)
    }

    if (values.length) {
      const val = values.shift()
      if (concatValue) {
        appendLast(words, val)
      } else {
        words.push(val)
      }
      lastWordWasValue = true
    } else {
      lastWordWasValue = false
    }
  }
  return words
}
