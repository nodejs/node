/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('./index.js').Context} Context
 */

import createDebug from 'debug'
import {statistics} from 'vfile-statistics'
import isBuffer from 'is-buffer'
import {inspectColor, inspectNoColor} from 'unist-util-inspect'

const debug = createDebug('unified-engine:file-pipeline:stringify')

/**
 * Stringify a tree.
 *
 * @param {Context} context
 * @param {VFile} file
 */
export function stringify(context, file) {
  /** @type {unknown} */
  let value

  if (statistics(file).fatal) {
    debug('Not compiling failed document')
    return
  }

  if (
    !context.settings.output &&
    !context.settings.out &&
    !context.settings.alwaysStringify
  ) {
    debug('Not compiling document without output settings')
    return
  }

  debug('Compiling `%s`', file.path)

  if (context.settings.inspect) {
    // Add a `txt` extension if there is a path.
    if (file.path) {
      file.extname = '.txt'
    }

    value =
      (context.settings.color ? inspectColor : inspectNoColor)(context.tree) +
      '\n'
  } else if (context.settings.treeOut) {
    // Add a `json` extension to ensure the file is correctly seen as JSON.
    // Only add it if there is a path — not if the file is for example stdin.
    if (file.path) {
      file.extname = '.json'
    }

    // Add the line feed to create a valid UNIX file.
    value = JSON.stringify(context.tree, null, 2) + '\n'
  } else {
    // @ts-expect-error: `tree` is defined if we came this far.
    value = context.processor.stringify(context.tree, file)
  }

  if (value === undefined || value === null) {
    // Empty.
  } else if (typeof value === 'string' || isBuffer(value)) {
    // @ts-expect-error: `isBuffer` checks buffer.
    file.value = value
  } else {
    file.result = value
  }

  debug('Serialized document')
}
