import fs from 'node:fs'
import path from 'node:path'
import {fault} from 'fault'
import createDebug from 'debug'
import {wrap} from 'trough'

const debug = createDebug('unified-engine:find-up')

/**
 * @template Value
 */
export class FindUp {
  /**
   * @callback Create
   * @param {Buffer} buf
   * @param {string} filePath
   * @returns {Promise<Value|undefined>|Value|undefined}
   */

  /**
   * @callback Callback
   * @param {Error|null} error
   * @param {Value} [result]
   * @returns {void}
   */

  /**
   * @typedef Options
   * @property {string} cwd
   * @property {string|undefined} filePath
   * @property {boolean|undefined} [detect]
   * @property {string[]} names
   * @property {Create} create
   */

  /**
   * @param {Options} options
   */
  constructor(options) {
    /** @type {Record<string, Callback[]|undefined|Error|Value>} */
    this.cache = {}
    /** @type {string} */
    this.cwd = options.cwd
    /** @type {boolean|undefined} */
    this.detect = options.detect
    /** @type {string[]} */
    this.names = options.names
    /** @type {Create} */
    this.create = options.create

    /** @type {string|undefined} */
    this.givenFilePath = options.filePath
      ? path.resolve(options.cwd, options.filePath)
      : undefined

    /* eslint-disable no-unused-expressions */
    /** @type {Error|Value|Callback[]|undefined} */
    this.givenFile
    /* eslint-enable no-unused-expressions */
  }

  /**
   * @param {string} filePath
   * @param {Callback} callback
   */
  load(filePath, callback) {
    const self = this
    const givenFile = this.givenFile
    const {givenFilePath} = this

    if (givenFilePath) {
      if (givenFile) {
        apply(callback, givenFile)
      } else {
        const cbs = [callback]
        this.givenFile = cbs
        debug('Checking given file `%s`', givenFilePath)
        fs.readFile(givenFilePath, (error, buf) => {
          if (error) {
            /** @type {NodeJS.ErrnoException} */
            const result = fault(
              'Cannot read given file `%s`\n%s',
              path.relative(this.cwd, givenFilePath),
              error.stack
            )
            result.code = 'ENOENT'
            result.path = error.path
            result.syscall = error.syscall
            loaded(result)
          } else {
            wrap(this.create, (error, /** @type {Value} */ result) => {
              if (error) {
                debug(error.message)
                loaded(
                  fault(
                    'Cannot parse given file `%s`\n%s',
                    path.relative(this.cwd, givenFilePath),
                    error.stack
                  )
                )
              } else {
                debug('Read given file `%s`', givenFilePath)
                loaded(result)
              }
            })(buf, givenFilePath)
          }

          /** @param {Error|Value} result */
          function loaded(result) {
            self.givenFile = result
            applyAll(cbs, result)
          }
        })
      }

      return
    }

    if (!this.detect) {
      return callback(null)
    }

    filePath = path.resolve(this.cwd, filePath)
    const parent = path.dirname(filePath)

    if (parent in this.cache) {
      apply(callback, this.cache[parent])
    } else {
      this.cache[parent] = [callback]
      find(parent)
    }

    /**
     * @param {string} directory
     */
    function find(directory) {
      let index = -1

      next()

      function next() {
        // Try to read the next file.
        // We do not use `readdir` because on huge directories, that could be
        // *very* slow.
        if (++index < self.names.length) {
          fs.readFile(path.join(directory, self.names[index]), done)
        } else {
          const parent = path.dirname(directory)

          if (directory === parent) {
            debug('No files found for `%s`', filePath)
            found(null)
          } else if (parent in self.cache) {
            apply(found, self.cache[parent])
          } else {
            self.cache[parent] = [found]
            find(parent)
          }
        }
      }

      /**
       * @param {NodeJS.ErrnoException|null} error
       * @param {Buffer} [buf]
       * @returns {void}
       */
      function done(error, buf) {
        const fp = path.join(directory, self.names[index])

        if (error) {
          // Hard to test.
          /* c8 ignore next 13 */
          if (error.code === 'ENOENT') {
            return next()
          }

          debug(error.message)
          return found(
            fault(
              'Cannot read file `%s`\n%s',
              path.relative(self.cwd, fp),
              error.message
            )
          )
        }

        wrap(self.create, (error, /** @type {Value} */ result) => {
          if (error) {
            found(
              fault(
                'Cannot parse file `%s`\n%s',
                path.relative(self.cwd, fp),
                error.message
              )
            )
          } else if (result) {
            debug('Read file `%s`', fp)
            found(null, result)
          } else {
            next()
          }
        })(buf, fp)
      }

      /**
       * @param {Error|null} error
       * @param {Value} [result]
       * @returns {void}
       */
      function found(error, result) {
        /** @type {Callback[]} */
        // @ts-expect-error: always a list if found.
        const cbs = self.cache[directory]
        self.cache[directory] = error || result
        applyAll(cbs, error || result)
      }
    }

    /**
     * @param {Callback[]} cbs
     * @param {Value|Error|undefined} result
     */
    function applyAll(cbs, result) {
      let index = cbs.length

      while (index--) {
        apply(cbs[index], result)
      }
    }

    /**
     * @param {Callback} cb
     * @param {Value|Error|Callback[]|undefined} result
     */
    function apply(cb, result) {
      if (Array.isArray(result)) {
        result.push(cb)
      } else if (result instanceof Error) {
        cb(result)
      } else {
        cb(null, result)
      }
    }
  }
}
