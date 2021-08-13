/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('trough').Callback} Callback
 * @typedef {import('./index.js').Context} Context
 */

import createDebug from 'debug'
import {statistics} from 'vfile-statistics'

const debug = createDebug('unified-engine:file-pipeline:queue')

const own = {}.hasOwnProperty

/**
 * Queue all files which came this far.
 * When the last file gets here, run the file-set pipeline and flush the queue.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function queue(context, file, next) {
  let origin = file.history[0]
  // @ts-expect-error: store a completion map on the `fileSet`.
  let map = context.fileSet.complete
  let complete = true

  if (!map) {
    map = {}
    // @ts-expect-error: store a completion map on the `fileSet`.
    context.fileSet.complete = map
  }

  debug('Queueing `%s`', origin)

  map[origin] = next

  const files = context.fileSet.valueOf()
  let index = -1
  while (++index < files.length) {
    each(files[index])
  }

  if (!complete) {
    debug('Not flushing: some files cannot be flushed')
    return
  }

  // @ts-expect-error: Reset map.
  context.fileSet.complete = {}
  context.fileSet.pipeline.run(context.fileSet, done)

  /**
   * @param {VFile} file
   */
  function each(file) {
    const key = file.history[0]

    if (statistics(file).fatal) {
      return
    }

    if (typeof map[key] === 'function') {
      debug('`%s` can be flushed', key)
    } else {
      debug('Interupting flush: `%s` is not finished', key)
      complete = false
    }
  }

  /**
   * @param {Error|Null} error
   */
  function done(error) {
    debug('Flushing: all files can be flushed')

    // Flush.
    for (origin in map) {
      if (own.call(map, origin)) {
        map[origin](error)
      }
    }
  }
}
