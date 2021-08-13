/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('trough').Callback} Callback
 * @typedef {import('./index.js').Settings} Settings
 * @typedef {import('./index.js').Configuration} Configuration
 */

/**
 * @typedef Context
 * @property {Array.<VFile>} files
 * @property {Configuration} configuration
 * @property {FileSet} fileSet
 */

import {FileSet} from '../file-set.js'
import {filePipeline} from '../file-pipeline/index.js'

/**
 * Transform all files.
 *
 * @param {Context} context
 * @param {Settings} settings
 * @param {Callback} next
 */
export function transform(context, settings, next) {
  const fileSet = new FileSet()

  context.fileSet = fileSet

  fileSet.on('add', (/** @type {VFile} */ file) => {
    filePipeline.run(
      {
        configuration: context.configuration,
        // Needed `any`s
        // type-coverage:ignore-next-line
        processor: settings.processor(),
        fileSet,
        settings
      },
      file,
      (/** @type {Error|null} */ error) => {
        // Does not occur as all failures in `filePipeLine` are failed on each
        // file.
        // Still, just to ensure things work in the future, we add an extra check.
        /* c8 ignore next 4 */
        if (error) {
          Object.assign(file.message(error), {fatal: true})
        }

        fileSet.emit('one', file)
      }
    )
  })

  fileSet.on('done', next)

  if (context.files.length === 0) {
    next()
  } else {
    let index = -1
    while (++index < context.files.length) {
      fileSet.add(context.files[index])
    }
  }
}
