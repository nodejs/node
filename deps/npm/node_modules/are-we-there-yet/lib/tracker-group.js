'use strict'
const TrackerBase = require('./tracker-base.js')
const Tracker = require('./tracker.js')
const TrackerStream = require('./tracker-stream.js')

class TrackerGroup extends TrackerBase {
  parentGroup = null
  trackers = []
  completion = {}
  weight = {}
  totalWeight = 0
  finished = false
  bubbleChange = bubbleChange(this)

  nameInTree () {
    var names = []
    var from = this
    while (from) {
      names.unshift(from.name)
      from = from.parentGroup
    }
    return names.join('/')
  }

  addUnit (unit, weight) {
    if (unit.addUnit) {
      var toTest = this
      while (toTest) {
        if (unit === toTest) {
          throw new Error(
            'Attempted to add tracker group ' +
            unit.name + ' to tree that already includes it ' +
            this.nameInTree(this))
        }
        toTest = toTest.parentGroup
      }
      unit.parentGroup = this
    }
    this.weight[unit.id] = weight || 1
    this.totalWeight += this.weight[unit.id]
    this.trackers.push(unit)
    this.completion[unit.id] = unit.completed()
    unit.on('change', this.bubbleChange)
    if (!this.finished) {
      this.emit('change', unit.name, this.completion[unit.id], unit)
    }
    return unit
  }

  completed () {
    if (this.trackers.length === 0) {
      return 0
    }
    var valPerWeight = 1 / this.totalWeight
    var completed = 0
    for (var ii = 0; ii < this.trackers.length; ii++) {
      var trackerId = this.trackers[ii].id
      completed +=
        valPerWeight * this.weight[trackerId] * this.completion[trackerId]
    }
    return completed
  }

  newGroup (name, weight) {
    return this.addUnit(new TrackerGroup(name), weight)
  }

  newItem (name, todo, weight) {
    return this.addUnit(new Tracker(name, todo), weight)
  }

  newStream (name, todo, weight) {
    return this.addUnit(new TrackerStream(name, todo), weight)
  }

  finish () {
    this.finished = true
    if (!this.trackers.length) {
      this.addUnit(new Tracker(), 1, true)
    }
    for (var ii = 0; ii < this.trackers.length; ii++) {
      var tracker = this.trackers[ii]
      tracker.finish()
      tracker.removeListener('change', this.bubbleChange)
    }
    this.emit('change', this.name, 1, this)
  }

  debug (depth = 0) {
    const indent = ' '.repeat(depth)
    let output = `${indent}${this.name || 'top'}: ${this.completed()}\n`

    this.trackers.forEach(function (tracker) {
      output += tracker instanceof TrackerGroup
        ? tracker.debug(depth + 1)
        : `${indent} ${tracker.name}: ${tracker.completed()}\n`
    })
    return output
  }
}

function bubbleChange (trackerGroup) {
  return function (name, completed, tracker) {
    trackerGroup.completion[tracker.id] = completed
    if (trackerGroup.finished) {
      return
    }
    trackerGroup.emit('change', name || trackerGroup.name, trackerGroup.completed(), trackerGroup)
  }
}

module.exports = TrackerGroup
