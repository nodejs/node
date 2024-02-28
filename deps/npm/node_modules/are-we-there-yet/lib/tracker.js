'use strict'
const TrackerBase = require('./tracker-base.js')

class Tracker extends TrackerBase {
  constructor (name, todo) {
    super(name)
    this.workDone = 0
    this.workTodo = todo || 0
  }

  completed () {
    return this.workTodo === 0 ? 0 : this.workDone / this.workTodo
  }

  addWork (work) {
    this.workTodo += work
    this.emit('change', this.name, this.completed(), this)
  }

  completeWork (work) {
    this.workDone += work
    if (this.workDone > this.workTodo) {
      this.workDone = this.workTodo
    }
    this.emit('change', this.name, this.completed(), this)
  }

  finish () {
    this.workTodo = this.workDone = 1
    this.emit('change', this.name, 1, this)
  }
}

module.exports = Tracker
