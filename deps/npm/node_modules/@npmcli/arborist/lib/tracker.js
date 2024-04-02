const npmlog = require('npmlog')

module.exports = cls => class Tracker extends cls {
  #progress = new Map()
  #setProgress

  constructor (options = {}) {
    super(options)
    this.#setProgress = !!options.progress
  }

  addTracker (section, subsection = null, key = null) {
    if (section === null || section === undefined) {
      this.#onError(`Tracker can't be null or undefined`)
    }

    if (key === null) {
      key = subsection
    }

    const hasTracker = this.#progress.has(section)
    const hasSubtracker = this.#progress.has(`${section}:${key}`)

    if (hasTracker && subsection === null) {
      // 0. existing tracker, no subsection
      this.#onError(`Tracker "${section}" already exists`)
    } else if (!hasTracker && subsection === null) {
      // 1. no existing tracker, no subsection
      // Create a new tracker from npmlog
      // starts progress bar
      if (this.#setProgress && this.#progress.size === 0) {
        npmlog.enableProgress()
      }

      this.#progress.set(section, npmlog.newGroup(section))
    } else if (!hasTracker && subsection !== null) {
      // 2. no parent tracker and subsection
      this.#onError(`Parent tracker "${section}" does not exist`)
    } else if (!hasTracker || !hasSubtracker) {
      // 3. existing parent tracker, no subsection tracker
      // Create a new subtracker in this.#progress from parent tracker
      this.#progress.set(`${section}:${key}`,
        this.#progress.get(section).newGroup(`${section}:${subsection}`)
      )
    }
    // 4. existing parent tracker, existing subsection tracker
    // skip it
  }

  finishTracker (section, subsection = null, key = null) {
    if (section === null || section === undefined) {
      this.#onError(`Tracker can't be null or undefined`)
    }

    if (key === null) {
      key = subsection
    }

    const hasTracker = this.#progress.has(section)
    const hasSubtracker = this.#progress.has(`${section}:${key}`)

    // 0. parent tracker exists, no subsection
    // Finish parent tracker and remove from this.#progress
    if (hasTracker && subsection === null) {
      // check if parent tracker does
      // not have any remaining children
      const keys = this.#progress.keys()
      for (const key of keys) {
        if (key.match(new RegExp(section + ':'))) {
          this.finishTracker(section, key)
        }
      }

      // remove parent tracker
      this.#progress.get(section).finish()
      this.#progress.delete(section)

      // remove progress bar if all
      // trackers are finished
      if (this.#setProgress && this.#progress.size === 0) {
        npmlog.disableProgress()
      }
    } else if (!hasTracker && subsection === null) {
      // 1. no existing parent tracker, no subsection
      this.#onError(`Tracker "${section}" does not exist`)
    } else if (!hasTracker || hasSubtracker) {
      // 2. subtracker exists
      // Finish subtracker and remove from this.#progress
      this.#progress.get(`${section}:${key}`).finish()
      this.#progress.delete(`${section}:${key}`)
    }
    // 3. existing parent tracker, no subsection
  }

  #onError (msg) {
    if (this.#setProgress) {
      npmlog.disableProgress()
    }
    throw new Error(msg)
  }
}
