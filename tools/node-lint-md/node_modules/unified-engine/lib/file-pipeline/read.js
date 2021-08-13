/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('trough').Callback} Callback
 * @typedef {import('./index.js').Context} Context
 */

import fs from 'node:fs'
import path from 'node:path'
import createDebug from 'debug'
import {statistics} from 'vfile-statistics'

const debug = createDebug('unified-engine:file-pipeline:read')

/**
 * Fill a file with its value when not already filled.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function read(context, file, next) {
  let filePath = file.path

  if (file.value || file.data.unifiedEngineStreamIn) {
    debug('Not reading file `%s` with `value`', filePath)
    next()
  } else if (statistics(file).fatal) {
    debug('Not reading failed file `%s`', filePath)
    next()
  } else {
    filePath = path.resolve(context.settings.cwd, filePath)

    debug('Reading `%s` in `%s`', filePath, 'utf8')
    fs.readFile(filePath, 'utf8', (error, value) => {
      debug('Read `%s` (error: %s)', filePath, error)

      file.value = value || ''

      next(error)
    })
  }
}
