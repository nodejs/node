const ansi = require('ansi-styles')

const colors = {
  removed: ansi.red,
  added: ansi.green,
  header: ansi.yellow,
  section: ansi.magenta
}

function colorize (str, opts) {
  let headerLength = (opts || {}).headerLength
  if (typeof headerLength !== 'number' || Number.isNaN(headerLength)) {
    headerLength = 2
  }

  const color = (str, colorId) => {
    const { open, close } = colors[colorId]
    // avoid highlighting the "\n" (would highlight till the end of the line)
    return str.replace(/[^\n\r]+/g, open + '$&' + close)
  }

  // this RegExp will include all the `\n` chars into the lines, easier to join
  const lines = ((typeof str === 'string' && str) || '').split(/^/m)

  const start = color(lines.slice(0, headerLength).join(''), 'header')
  const end = lines.slice(headerLength).join('')
    .replace(/^-.*/gm, color('$&', 'removed'))
    .replace(/^\+.*/gm, color('$&', 'added'))
    .replace(/^@@.+@@/gm, color('$&', 'section'))

  return start + end
}

module.exports = colorize
