'use strict'

module.exports = filter
function filter (data, include, exclude, opts) {
  return typeof data === 'object' &&
         filterWords(data, include, exclude, opts)
}

function getWords (data, opts) {
  return [ data.name ]
    .concat((opts && opts.description) ? data.description : [])
    .concat((data.maintainers || []).map(function (m) {
      return '=' + m.name
    }))
    .concat(data.versions && data.versions.length && data.url && ('<' + data.url + '>'))
    .concat(data.keywords || [])
    .map(function (f) { return f && f.trim && f.trim() })
    .filter(function (f) { return f })
    .join(' ')
    .toLowerCase()
}

function filterWords (data, include, exclude, opts) {
  var words = getWords(data, opts)
  for (var i = 0, l = include.length; i < l; i++) {
    if (!match(words, include[i])) return false
  }
  for (i = 0, l = exclude.length; i < l; i++) {
    if (match(words, exclude[i])) return false
  }
  return true
}

function match (words, pattern) {
  if (pattern.charAt(0) === '/') {
    pattern = pattern.replace(/\/$/, '')
    pattern = new RegExp(pattern.substr(1, pattern.length - 1))
    return words.match(pattern)
  }
  return words.indexOf(pattern) !== -1
}
