/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('trough').Pipeline} Pipeline
 */

/**
 * @callback CompleterCallback
 * @param {FileSet} set
 * @param {(error?: Error|null) => void} callback
 * @returns {void}
 *
 * @callback CompleterAsync
 * @param {FileSet} set
 * @returns {Promise<void>}
 *
 * @callback CompleterSync
 * @param {FileSet} set
 * @returns {void}
 *
 * @typedef {(CompleterCallback|CompleterAsync|CompleterSync) & {pluginId?: string}} Completer
 */

import {EventEmitter} from 'node:events'
import {trough} from 'trough'
import {toVFile} from 'to-vfile'

export class FileSet extends EventEmitter {
  /**
   * FileSet constructor.
   * A FileSet is created to process multiple files through unified processors.
   * This set, containing all files, is exposed to plugins as an argument to the
   * attacher.
   */
  constructor() {
    super()

    /** @type {Array.<VFile>} */
    this.files = []
    /** @type {string[]} */
    this.origins = []
    /** @type {Completer[]} */
    this.plugins = []
    /** @type {number} */
    this.expected = 0
    /** @type {number} */
    this.actual = 0
    /** @type {Pipeline} */
    this.pipeline = trough()

    // Called when a single file has completed it’s pipeline, triggering `done`
    // when all files are complete.
    this.on('one', () => {
      this.actual++

      if (this.actual >= this.expected) {
        this.emit('done')
      }
    })
  }

  /**
   * Access the files in a set.
   */
  valueOf() {
    return this.files
  }

  /**
   * Attach middleware to the pipeline on `fileSet`.
   *
   * @param {Completer} plugin
   */
  use(plugin) {
    const pipeline = this.pipeline
    let duplicate = false

    if (plugin && plugin.pluginId) {
      duplicate = this.plugins.some((fn) => fn.pluginId === plugin.pluginId)
    }

    if (!duplicate && this.plugins.includes(plugin)) {
      duplicate = true
    }

    if (!duplicate) {
      this.plugins.push(plugin)
      pipeline.use(plugin)
    }

    return this
  }

  /**
   * Add a file to be processed.
   * The given file is processed like other files with a few differences:
   *
   * *   Ignored when their file path is already added
   * *   Never written to the file system or streamOut
   * *   Not reported for
   *
   * @param {string|VFile} file
   */
  add(file) {
    if (typeof file === 'string') {
      file = toVFile(file)
    }

    // Prevent files from being added multiple times.
    if (this.origins.includes(file.history[0])) {
      return this
    }

    this.origins.push(file.history[0])

    // Add.
    this.valueOf().push(file)
    this.expected++

    // Force an asynchronous operation.
    // This ensures that files which fall through the file pipeline immediately
    // (such as, when already fatally failed) still queue up correctly.
    setImmediate(() => {
      this.emit('add', file)
    })

    return this
  }
}
