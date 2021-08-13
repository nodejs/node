/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('trough').Callback} Callback
 * @typedef {import('./index.js').Context} Context
 */

import fs from 'node:fs'
import path from 'node:path'
import createDebug from 'debug'
import {statistics} from 'vfile-statistics'

const debug = createDebug('unified-engine:file-pipeline:file-system')

/**
 * Write a virtual file to the file-system.
 * Ignored when `output` is not given.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function fileSystem(context, file, next) {
  if (!context.settings.output) {
    debug('Ignoring writing to file-system')
    return next()
  }

  if (!file.data.unifiedEngineGiven) {
    debug('Ignoring programmatically added file')
    return next()
  }

  let destinationPath = file.path

  if (!destinationPath) {
    debug('Cannot write file without a `destinationPath`')
    return next(new Error('Cannot write file without an output path'))
  }

  if (statistics(file).fatal) {
    debug('Cannot write file with a fatal error')
    return next()
  }

  destinationPath = path.resolve(context.settings.cwd, destinationPath)
  debug('Writing document to `%s`', destinationPath)

  file.stored = true
  fs.writeFile(destinationPath, file.toString(), next)
}
