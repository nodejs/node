"use strict"
var test = require("tap").test
var ProgressBar = require("../progress-bar.js")

var cursor = []
var C
var bar = new ProgressBar({theme: ProgressBar.ascii, maxUpdateFrequency: 0}, C = {
  show: function () {
    cursor.push(["show"])
    return C
  },
  hide: function () {
    cursor.push(["hide"])
    return C
  },
  up: function (lines) {
    cursor.push(["up",lines])
    return C
  },
  horizontalAbsolute: function (col) {
    cursor.push(["horizontalAbsolute", col])
    return C
  },
  eraseLine: function () {
    cursor.push(["eraseLine"])
    return C
  },
  write: function (line) {
    cursor.push(["write", line])
    return C
  }
})


function isOutput(t, msg, output) {
  var tests = []
  for (var ii = 0; ii<output.length; ++ii) {
    for (var jj = 0; jj<output[ii].length; ++jj) {
      tests.push({cmd: ii, arg: jj})
    }
  }
  tests.forEach(function(test) {
    t.is(cursor[test.cmd] ? cursor[test.cmd][test.arg] : null,
         output[test.cmd][test.arg],
         msg + ": " + output[test.cmd] + (test.arg ? " arg #"+test.arg : ""))
  })
}

test("hide", function (t) {
  t.plan(11)
  process.stderr.isTTY = false
  bar.hide()
  t.is(cursor.length, 0, "We don't progress bar without a tty")
  cursor = []
  process.stderr.isTTY = true
  bar.hide()
  isOutput(t, "hide while not showing",[
    ["show"], // cursor
    ["horizontalAbsolute",0],
    ["eraseLine"]])
  cursor = []
  bar.showing = true
  bar.hide()
  isOutput(t, "hide while showing",[
    ["show"], // cursor
    ["up", 1],
    ["horizontalAbsolute",0],
    ["eraseLine"]])
})

test("renderTemplate", function (t) {
  t.plan(16)
  process.stdout.columns = 11
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "name"}],{name: "NAME"})
  t.is(result, "NAME", "name substitution")
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "completionbar"}],{completed: 0})
  t.is(result, "----------", "0% bar")
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "completionbar"}],{completed: 0.5})
  t.is(result, "#####-----", "50% bar")
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "completionbar"}],{completed: 1})
  t.is(result, "##########", "100% bar")
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "completionbar"}],{completed: -100})
  t.is(result, "----------", "0% underflow bar")
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "completionbar"}],{completed: 100})
  t.is(result, "##########", "100% overflow bar")
  var result = bar.renderTemplate(ProgressBar.ascii,[{type: "name"},{type: "completionbar"}],{name: "NAME", completed: 0.5})
  t.is(result, "NAME###---", "name + 50%")
  var result = bar.renderTemplate(ProgressBar.ascii, ["static"], {})
  t.is(result, "static", "static text")
  var result = bar.renderTemplate(ProgressBar.ascii, ["static",{type: "name"}], {name: "NAME"})
  t.is(result, "staticNAME", "static text + var")
  var result = bar.renderTemplate(ProgressBar.ascii, ["static",{type: "name", separated: true}], {name: "NAME"})
  t.is(result, "static NAME ", "pre-separated")
  var result = bar.renderTemplate(ProgressBar.ascii, [{type: "name", separated: true}, "static"], {name: "NAME"})
  t.is(result, "NAME static", "post-separated")
  var result = bar.renderTemplate(ProgressBar.ascii, ["1",{type: "name", separated: true}, "2"], {name: ""})
  t.is(result, "12", "separated no value")
  var result = bar.renderTemplate(ProgressBar.ascii, ["1",{type: "name", separated: true}, "2"], {name: "NAME"})
  t.is(result, "1 NAME 2", "separated value")
  var result = bar.renderTemplate(ProgressBar.ascii, [{type: "spinner"}], {spinner: 0})
  t.is(result, "", "No spinner")
  var result = bar.renderTemplate(ProgressBar.ascii, [{type: "spinner"}], {spinner: 1})
  t.is(result, "\\", "Spinner 1")
  var result = bar.renderTemplate(ProgressBar.ascii, [{type: "spinner"}], {spinner: 10})
  t.is(result, "|", "Spinner 10")
})

test("show & pulse", function (t) {
  t.plan(23)

  process.stdout.columns = 16
  cursor = []
  process.stderr.isTTY = false
  bar.template[0].length = 6
  bar.last = new Date(0)
  bar.show("NAME", 0)
  t.is(cursor.length, 0, "no tty, no progressbar")

  cursor = []
  process.stderr.isTTY = true
  bar.last = new Date(0)
  bar.show("NAME", 0.1)
  isOutput(t, "tty, name, completion",
    [ [ 'hide' ],
      [ 'horizontalAbsolute', 0 ],
      [ 'write', 'NAME   |#-----|\n' ],
      [ 'show' ] ])

  bar.show("S")
  cursor = []
  bar.last = new Date(0)
  bar.pulse()
  isOutput(t, "pulsed spinner",
    [ [ 'up', 1 ],
      [ 'hide' ],
      [ 'horizontalAbsolute', 0 ],
      [ 'write', 'S      \\ |----|\n' ],
      [ 'show' ] ])
  cursor = []
  bar.last = new Date(0)
  bar.pulse("P")
  isOutput(t, "pulsed spinner with subsection",
    [ [ 'up', 1 ],
      [ 'hide' ],
      [ 'horizontalAbsolute', 0 ],
      [ 'write', 'S -> P | |----|\n' ],
      [ 'show' ] ])
})

test("window resizing", function (t) {
  t.plan(16)
  process.stderr.isTTY = true
  process.stdout.columns = 32
  bar.show("NAME", 0.1)
  cursor = []
  bar.last = new Date(0)
  bar.pulse()
  isOutput(t, "32 columns",
    [ [ 'up', 1 ],
      [ 'hide' ],
      [ 'horizontalAbsolute', 0 ],
      [ 'write', 'NAME   / |##------------------|\n' ],
      [ 'show' ] ])

  process.stdout.columns = 16
  bar.show("NAME", 0.5)
  cursor = []
  bar.last = new Date(0)
  bar.pulse()
  isOutput(t, "16 columns",
    [ [ 'up', 1 ],
      [ 'hide' ],
      [ 'horizontalAbsolute', 0 ],
      [ 'write', 'NAME   - |##--|\n' ],
      [ 'show' ] ]);
});
