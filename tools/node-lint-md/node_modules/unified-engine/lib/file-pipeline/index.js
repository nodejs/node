/**
 * @typedef {import('trough').Pipeline} Pipeline
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('vfile-message').VFileMessage} VFileMessage
 * @typedef {import('unist').Node} Node
 * @typedef {import('unified').Processor} Processor
 * @typedef {import('../file-set.js').FileSet} FileSet
 * @typedef {import('../configuration.js').Configuration} Configuration
 * @typedef {import('../index.js').Settings} Settings
 */

/**
 * @typedef Context
 * @property {Processor} processor
 * @property {FileSet} fileSet
 * @property {Configuration} configuration
 * @property {Settings} settings
 * @property {Node} [tree]
 */

import {trough} from 'trough'
import {read} from './read.js'
import {configure} from './configure.js'
import {parse} from './parse.js'
import {transform} from './transform.js'
import {queue} from './queue.js'
import {stringify} from './stringify.js'
import {copy} from './copy.js'
import {stdout} from './stdout.js'
import {fileSystem} from './file-system.js'

// This pipeline ensures each of the pipes always runs: even if the read pipe
// fails, queue and write run.
export const filePipeline = trough()
  .use(chunk(trough().use(read).use(configure).use(parse).use(transform)))
  .use(chunk(trough().use(queue)))
  .use(chunk(trough().use(stringify).use(copy).use(stdout).use(fileSystem)))

/**
 * Factory to run a pipe.
 * Wraps a pipe to trigger an error on the `file` in `context`, but still call
 * `next`.
 *
 * @param {Pipeline} pipe
 */
function chunk(pipe) {
  return run

  /**
   * Run the bound pipe and handle any errors.
   *
   * @param {Context} context
   * @param {VFile} file
   * @param {() => void} next
   */
  function run(context, file, next) {
    pipe.run(context, file, (/** @type {VFileMessage|null} */ error) => {
      const messages = file.messages

      if (error) {
        const index = messages.indexOf(error)

        if (index === -1) {
          Object.assign(file.message(error), {fatal: true})
        } else {
          messages[index].fatal = true
        }
      }

      next()
    })
  }
}
