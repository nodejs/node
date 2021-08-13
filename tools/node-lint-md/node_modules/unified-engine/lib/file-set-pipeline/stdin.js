/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('trough').Callback} Callback
 * @typedef {import('./index.js').Settings} Settings
 */

/**
 * @typedef Context
 * @property {Array.<string|VFile>} files
 */

import createDebug from 'debug'
import concatStream from 'concat-stream'
import {toVFile} from 'to-vfile'

const debug = createDebug('unified-engine:file-set-pipeline:stdin')

/**
 * @param {Context} context
 * @param {Settings} settings
 * @param {Callback} next
 */
export function stdin(context, settings, next) {
  if (settings.files && settings.files.length > 0) {
    debug('Ignoring `streamIn`')

    /** @type {Error|undefined} */
    let error

    if (settings.filePath) {
      error = new Error(
        'Do not pass both `--file-path` and real files.\nDid you mean to pass stdin instead of files?'
      )
    }

    next(error)

    return
  }

  // @ts-expect-error: does exist on `stdin`.
  if (settings.streamIn.isTTY) {
    debug('Cannot read from `tty` stream')
    next(new Error('No input'))

    return
  }

  debug('Reading from `streamIn`')

  settings.streamIn.pipe(
    concatStream({encoding: 'string'}, (value) => {
      const file = toVFile(settings.filePath)

      debug('Read from `streamIn`')

      file.cwd = settings.cwd
      file.value = value
      file.data.unifiedEngineGiven = true
      file.data.unifiedEngineStreamIn = true

      context.files = [file]

      // If `out` was not set, set `out`.
      settings.out =
        settings.out === null || settings.out === undefined
          ? true
          : settings.out

      next()
    })
  )
}
