const procLog = require('./proc-log.js')

const _progress = Symbol('_progress')
const _onError = Symbol('_onError')

module.exports = cls => class Tracker extends cls {
  constructor (options = {}) {
    super(options)
    this[_progress] = new Map()
    this.log = options.log || procLog
  }

  addTracker (section, subsection = null, key = null) {
    // TrackerGroup type object not found
    if (!this.log.newGroup)
      return

    if (section === null || section === undefined)
      this[_onError](`Tracker can't be null or undefined`)

    if (key === null)
      key = subsection

    const hasTracker = this[_progress].has(section)
    const hasSubtracker = this[_progress].has(`${section}:${key}`)

    if (hasTracker && subsection === null)
      // 0. existing tracker, no subsection
      this[_onError](`Tracker "${section}" already exists`)

    else if (!hasTracker && subsection === null) {
      // 1. no existing tracker, no subsection
      // Create a new tracker from this.log
      // starts progress bar
      if (this[_progress].size === 0)
        this.log.enableProgress()

      this[_progress].set(section, this.log.newGroup(section))
    } else if (!hasTracker && subsection !== null)
      // 2. no parent tracker and subsection
      this[_onError](`Parent tracker "${section}" does not exist`)

    else if (!hasTracker || !hasSubtracker) {
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
    // TrackerGroup type object not found
    if (!this.log.newGroup)
      return

    if (section === null || section === undefined)
      this[_onError](`Tracker can't be null or undefined`)

    if (key === null)
      key = subsection

    const hasTracker = this[_progress].has(section)
    const hasSubtracker = this[_progress].has(`${section}:${key}`)

    // 0. parent tracker exists, no subsection
    // Finish parent tracker and remove from this[_progress]
    if (hasTracker && subsection === null) {
      // check if parent tracker does
      // not have any remaining children
      const keys = this[_progress].keys()
      for (const key of keys) {
        if (key.match(new RegExp(section + ':')))
          this.finishTracker(section, key)
      }

      // remove parent tracker
      this[_progress].get(section).finish()
      this[_progress].delete(section)

      // remove progress bar if all
      // trackers are finished
      if (this[_progress].size === 0)
        this.log.disableProgress()
    } else if (!hasTracker && subsection === null)
      // 1. no existing parent tracker, no subsection
      this[_onError](`Tracker "${section}" does not exist`)

    else if (!hasTracker || hasSubtracker) {
      // 2. subtracker exists
      // Finish subtracker and remove from this[_progress]
      this[_progress].get(`${section}:${key}`).finish()
      this[_progress].delete(`${section}:${key}`)
    }
    // 3. existing parent tracker, no subsection
  }

  [_onError] (msg) {
    this.log.disableProgress()
    throw new Error(msg)
  }
}
