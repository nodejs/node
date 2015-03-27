"use strict"
var stream = require("stream");
var EventEmitter = require("events").EventEmitter
var util = require("util")
var delegate = require("delegates")

var TrackerGroup = exports.TrackerGroup = function (name) {
  EventEmitter.call(this)
  this.name = name
  this.trackGroup = []
  var self = this
  this.totalWeight = 0
  var noteChange = this.noteChange = function (name) {
    self.emit("change", name || this.name)
  }.bind(this)
  this.trackGroup.forEach(function(unit) {
    unit.on("change", noteChange)
  })
}
util.inherits(TrackerGroup, EventEmitter)

TrackerGroup.prototype.completed = function () {
  if (this.trackGroup.length==0) return 0
  var valPerWeight = 1 / this.totalWeight
  var completed = 0
  this.trackGroup.forEach(function(T) {
    completed += valPerWeight * T.weight *  T.completed()
  })
  return completed
}

TrackerGroup.prototype.addUnit = function (unit, weight, noChange) {
  unit.weight = weight || 1
  this.totalWeight += unit.weight
  this.trackGroup.push(unit)
  unit.on("change", this.noteChange)
  if (! noChange) this.emit("change", this.name)
  return unit
}

TrackerGroup.prototype.newGroup = function (name, weight) {
  return this.addUnit(new TrackerGroup(name), weight)
}

TrackerGroup.prototype.newItem = function (name, todo, weight) {
  return this.addUnit(new Tracker(name, todo), weight)
}

TrackerGroup.prototype.newStream = function (name, todo, weight) {
  return this.addUnit(new TrackerStream(name, todo), weight)
}

TrackerGroup.prototype.finish = function () {
  if (! this.trackGroup.length) { this.addUnit(new Tracker(), 1, true) }
  var self = this
  this.trackGroup.forEach(function(T) {
    T.removeListener("change", self.noteChange)
    T.finish()
  })
  this.emit("change", this.name)
}

var buffer = "                                  "
TrackerGroup.prototype.debug = function (depth) {
  depth = depth || 0
  var indent = depth ? buffer.substr(0,depth) : ""
  var output = indent + (this.name||"top") + ": " + this.completed() + "\n"
  this.trackGroup.forEach(function(T) {
    if (T instanceof TrackerGroup) {
      output += T.debug(depth + 1)
    }
    else {
      output += indent + " " + T.name + ": " + T.completed() + "\n"
    }
  })
  return output
}

var Tracker = exports.Tracker = function (name,todo) {
  EventEmitter.call(this)
  this.name = name
  this.workDone =  0
  this.workTodo = todo || 0
}
util.inherits(Tracker, EventEmitter)

Tracker.prototype.completed = function () {
  return this.workTodo==0 ? 0 : this.workDone / this.workTodo
}

Tracker.prototype.addWork = function (work) {
  this.workTodo += work
  this.emit("change", this.name)
}

Tracker.prototype.completeWork = function (work) {
  this.workDone += work
  if (this.workDone > this.workTodo) this.workDone = this.workTodo
  this.emit("change", this.name)
}

Tracker.prototype.finish = function () {
  this.workTodo = this.workDone = 1
  this.emit("change", this.name)
}


var TrackerStream = exports.TrackerStream = function (name, size, options) {
  stream.Transform.call(this, options)
  this.tracker = new Tracker(name, size)
  this.name = name
  var self = this
  this.tracker.on("change", function (name) { self.emit("change", name) })
}
util.inherits(TrackerStream, stream.Transform)

TrackerStream.prototype._transform = function (data, encoding, cb) {
  this.tracker.completeWork(data.length ? data.length : 1)
  this.push(data)
  cb()
}

TrackerStream.prototype._flush = function (cb) {
  this.tracker.finish()
  cb()
}

delegate(TrackerStream.prototype, "tracker")
  .method("completed")
  .method("addWork")
