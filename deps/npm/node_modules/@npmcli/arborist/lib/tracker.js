const _progress = Symbol('_progress')
const _onError = Symbol('_onError')
const _setProgress = Symbol('_setProgess')
const npmlog = require('npmlog')

module.exports = cls => class Tracker extends cls {
  constructor (options = {}) {
    super(options)
    this[_setProgress] = !!options.progress
    this[_progress] = new Map()
  }

  addTracker (section, subsection = null, key = null) {
    if (section === null || section === undefined) {
      this[_onError](`Tracker can't be null or undefined`)
    }

    if (key === null) {
      key = subsection
    }

    const hasTracker = this[_progress].has(section)
    const hasSubtracker = this[_progress].has(`${section}:${key}`)

    if (hasTracker && subsection === null) {
      // 0. existing tracker, no subsection
      this[_onError](`Tracker "${section}" already exists`)
    } else if (!hasTracker && subsection === null) {
      // 1. no existing tracker, no subsection
      // Create a new tracker from npmlog
      // starts progress bar
      if (this[_setProgress] && this[_progress].size === 0) {
        npmlog.enableProgress()
      }

      this[_progress].set(section, npmlog.newGroup(section))
    } else if (!hasTracker && subsection !== null) {
      // 2. no parent tracker and subsection
      this[_onError](`Parent tracker "${section}" does not exist`)
    } else if (!hasTracker || !hasSubtracker) {
      // 3. existing parent tracker, no subsection tracker
      // Create a new subtracker in this[_progress] from parent tracker
      this[_progress].set(`${section}:${key}`,
        this[_progress].get(section).newGroup(`${section}:${subsection}`)
      )
    }
    // 4. existing parent tracker, existing subsection tracker
    // skip it
  }

  finishTracker (section, subsection = null, key = null) {
    if (section === null || section === undefined) {
      this[_onError](`Tracker can't be null or undefined`)
    }

    if (key === null) {
      key = subsection
    }

    const hasTracker = this[_progress].has(section)
    const hasSubtracker = this[_progress].has(`${section}:${key}`)

    // 0. parent tracker exists, no subsection
    // Finish parent tracker and remove from this[_progress]
    if (hasTracker && subsection === null) {
      // check if parent tracker does
      // not have any remaining children
      const keys = this[_progress].keys()
      for (const key of keys) {
        if (key.match(new RegExp(section + ':'))) {
          this.finishTracker(section, key)
        }
      }

      // remove parent tracker
      this[_progress].get(section).finish()
      this[_progress].delete(section)

      // remove progress bar if all
      // trackers are finished
      if (this[_setProgress] && this[_progress].size === 0) {
        npmlog.disableProgress()
      }
    } else if (!hasTracker && subsection === null) {
      // 1. no existing parent tracker, no subsection
      this[_onError](`Tracker "${section}" does not exist`)
    } else if (!hasTracker || hasSubtracker) {
      // 2. subtracker exists
      // Finish subtracker and remove from this[_progress]
      this[_progress].get(`${section}:${key}`).finish()
      this[_progress].delete(`${section}:${key}`)
    }
    // 3. existing parent tracker, no subsection
  }

  [_onError] (msg) {
    if (this[_setProgress]) {
      npmlog.disableProgress()
    }
    throw new Error(msg)
  }
}
