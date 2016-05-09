"use strict"
var hasUnicode = require("has-unicode")
var ansi = require("ansi")
var align = {
  center: require("lodash.pad"),
  left:   require("lodash.padend"),
  right:  require("lodash.padstart")
}
var defaultStream = process.stderr
function isTTY() {
  return process.stderr.isTTY
}
function getWritableTTYColumns() {
  // Writing to the final column wraps the line
  // We have to use stdout here, because Node's magic SIGWINCH handler only
  // updates process.stdout, not process.stderr
  return process.stdout.columns - 1
}

var ProgressBar = module.exports = function (options, cursor) {
  if (! options) options = {}
  if (! cursor && options.write) {
    cursor = options
    options = {}
  }
  if (! cursor) {
    cursor = ansi(defaultStream)
  }
  this.cursor = cursor
  this.showing = false
  this.theme = options.theme || (hasUnicode() ? ProgressBar.unicode : ProgressBar.ascii)
  this.template = options.template || [
    {type: "name", separated: true, length: 25},
    {type: "spinner", separated: true},
    {type: "startgroup"},
    {type: "completionbar"},
    {type: "endgroup"}
  ]
  this.updatefreq = options.maxUpdateFrequency == null ? 50 : options.maxUpdateFrequency
  this.lastName = ""
  this.lastCompleted = 0
  this.spun = 0
  this.last = new Date(0)

  var self = this
  this._handleSizeChange = function () {
    if (!self.showing) return
    self.hide()
    self.show()
  }
}
ProgressBar.prototype = {}

ProgressBar.unicode = {
  startgroup: "╢",
  endgroup: "╟",
  complete: "█",
  incomplete: "░",
  spinner: "▀▐▄▌",
  subsection: "→"
}

ProgressBar.ascii = {
  startgroup: "|",
  endgroup: "|",
  complete: "#",
  incomplete: "-",
  spinner: "-\\|/",
  subsection: "->"
}

ProgressBar.prototype.setTheme = function(theme) {
  this.theme = theme
}

ProgressBar.prototype.setTemplate = function(template) {
  this.template = template
}

ProgressBar.prototype._enableResizeEvents = function() {
  process.stdout.on('resize', this._handleSizeChange)
}

ProgressBar.prototype._disableResizeEvents = function() {
  process.stdout.removeListener('resize', this._handleSizeChange)
}

ProgressBar.prototype.disable = function() {
  this.hide()
  this.disabled = true
}

ProgressBar.prototype.enable = function() {
  this.disabled = false
  this.show()
}

ProgressBar.prototype.hide = function() {
  if (!isTTY()) return
  if (this.disabled) return
  this.cursor.show()
  if (this.showing) this.cursor.up(1)
  this.cursor.horizontalAbsolute(0).eraseLine()
  this.showing = false
}

var repeat = function (str, count) {
  var out = ""
  for (var ii=0; ii<count; ++ii) out += str
  return out
}

ProgressBar.prototype.pulse = function(name) {
  ++ this.spun
  if (! this.showing) return
  if (this.disabled) return

  var baseName = this.lastName
  name = name
       ? ( baseName
         ? baseName + " " + this.theme.subsection + " " + name
         : null )
       : baseName
  this.show(name)
  this.lastName = baseName
}

ProgressBar.prototype.show = function(name, completed) {
  name = this.lastName = name || this.lastName
  completed = this.lastCompleted = completed || this.lastCompleted

  if (!isTTY()) return
  if (this.disabled) return
  if (! this.spun && ! completed) return
  if (this.tryAgain) return
  var self = this

  if (this.showing && new Date() - this.last < this.updatefreq) {
    this.tryAgain = setTimeout(function () {
      self.tryAgain = null
      if (self.disabled) return
      if (! self.spun && ! completed) return
      drawBar()
    }, this.updatefreq - (new Date() - this.last))
    return
  }

  return drawBar()

  function drawBar() {
    var values = {
      name: name,
      spinner: self.spun,
      completed: completed
    }

    self.last = new Date()

    var statusline = self.renderTemplate(self.theme, self.template, values)

    if (self.showing) self.cursor.up(1)
    self.cursor
        .hide()
        .horizontalAbsolute(0)
        .write(statusline.substr(0, getWritableTTYColumns()) + "\n")
        .show()

    self.showing = true
  }
}

ProgressBar.prototype.renderTemplate = function (theme, template, values) {
  values.startgroup = theme.startgroup
  values.endgroup = theme.endgroup
  values.spinner = values.spinner
    ? theme.spinner.substr(values.spinner % theme.spinner.length,1)
    : ""

  var output = {prebar: "", postbar: ""}
  var status = "prebar"
  var self = this
  template.forEach(function(T) {
    if (typeof T === "string") {
      output[status] += T
      return
    }
    if (T.type === "completionbar") {
      status = "postbar"
      return
    }
    if (!values.hasOwnProperty(T.type)) throw new Error("Unknown template value '"+T.type+"'")
    var value = self.renderValue(T, values[T.type])
    if (value === "") return
    var sofar = output[status].length
    var lastChar = sofar ? output[status][sofar-1] : null
    if (T.separated && sofar && lastChar !== " ") {
      output[status] += " "
    }
    output[status] += value
    if (T.separated) output[status] += " "
  })

  var bar = ""
  if (status === "postbar") {
    var nonBarLen = output.prebar.length + output.postbar.length

    var barLen = getWritableTTYColumns() - nonBarLen
    var sofar = Math.round(barLen * Math.max(0,Math.min(1,values.completed||0)))
    var rest = barLen - sofar
    bar = repeat(theme.complete, sofar)
        + repeat(theme.incomplete, rest)
  }

  return output.prebar + bar + output.postbar
}
ProgressBar.prototype.renderValue = function (template, value) {
  if (value == null || value === "") return ""
  var maxLength = template.maxLength || template.length
  var minLength = template.minLength || template.length
  var alignWith = align[template.align] || align.left
//  if (maxLength) value = value.substr(-1 * maxLength)
  if (maxLength) value = value.substr(0, maxLength)
  if (minLength) value = alignWith(value, minLength)
  return value
}
