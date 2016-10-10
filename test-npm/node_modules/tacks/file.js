'use strict'
var path = require('path')
var fs = require('fs')
var inherits = require('util').inherits
var Entry = require('./entry')
var asLiteral = require('./as-literal.js')

module.exports = File

function File (contents) {
  if (this == null) return new File(contents)
  this.type = 'file'
  if (typeof contents === 'object' && !Buffer.isBuffer(contents)) {
    contents = JSON.stringify(contents)
  }
  Entry.call(this, 'file', contents)
}
inherits(File, Entry)

File.prototype.create = function (where) {
  fs.writeFileSync(path.resolve(where, this.path), this.contents)
}

function tryJSON (str) {
  try {
    return JSON.parse(str)
  } catch (ex) {
    return
  }
}

File.prototype.toSource = function () {
  if (this.contents.length === 0) return "File('')"
  var output = 'File('
  var fromJson = tryJSON(this.contents)
  if (fromJson != null) {
    var jsonStr = asLiteral(fromJson)
    if (/^[\[{]/.test(jsonStr)) {
      output += jsonStr.replace(/\n/g, '\n  ')
                       .replace(/[ ]{2}([}\]])$/, '$1)')
    } else {
      output += jsonStr + '\n  )'
    }
  } else if (/[^\-\w\s~`!@#$%^&*()_=+[\]{}|\\;:'",./<>?]/.test(this.contents.toString())) {
    output += outputAsBuffer(this.contents)
        .replace(/[)]$/, '\n))')
  } else {
    output += outputAsText(this.contents) +
        '\n)'
  }
  return output
}

function outputAsText (content) {
  content = content.toString('utf8')
  var endsInNewLine = /\n$/.test(content)
  var lines = content.split(/\n/).map(function (line) { return line + '\n' })
  if (endsInNewLine) lines.pop()
  var output = '\n  ' + asLiteral(lines.shift())
  lines.forEach(function (line) {
    output += ' +\n  ' + asLiteral(line)
  })
  return output
}

function outputAsBuffer (content) {
  var chunks = content.toString('hex').match(/.{1,60}/g)
  var output = 'new Buffer(\n'
  output += "  '" + chunks.shift() + "'"
  chunks.forEach(function (chunk) {
    output += ' +\n  ' + "'" + chunk + "'"
  })
  output += ",\n  'hex')"
  return output
}
