function ansiTrim (str) {
  var r = new RegExp('\x1b(?:\\[(?:\\d+[ABCDEFGJKSTm]|\\d+;\\d+[Hfm]|' +
        '\\d+;\\d+;\\d+m|6n|s|u|\\?25[lh])|\\w)', 'g')
  return str.replace(r, '')
}

module.exports = ansiTrim
