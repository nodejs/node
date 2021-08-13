/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('../index.js').VFileReporter} VFileReporter
 * @typedef {import('./index.js').Settings} Settings
 * @typedef {import('./index.js').Configuration} Configuration
 */

import {loadPlugin} from 'load-plugin'
import {reporter} from 'vfile-reporter'

/**
 * @typedef Context
 * @property {Array.<VFile>} files
 * @property {Configuration} [configuration]
 */

/**
 * @param {Context} context
 * @param {Settings} settings
 */
export async function log(context, settings) {
  /** @type {VFileReporter} */
  let func = reporter

  if (typeof settings.reporter === 'string') {
    try {
      // @ts-expect-error: Assume loaded value is a vfile reporter.
      func = await loadPlugin(settings.reporter, {
        cwd: settings.cwd,
        prefix: 'vfile-reporter'
      })
    } catch {
      throw new Error('Could not find reporter `' + settings.reporter + '`')
    }
  } else if (settings.reporter) {
    func = settings.reporter
  }

  let diagnostics = func(
    context.files.filter((file) => file.data.unifiedEngineGiven),
    Object.assign({}, settings.reporterOptions, {
      quiet: settings.quiet,
      silent: settings.silent,
      color: settings.color
    })
  )

  if (diagnostics) {
    if (diagnostics.charAt(diagnostics.length - 1) !== '\n') {
      diagnostics += '\n'
    }

    return new Promise((resolve) => {
      settings.streamError.write(diagnostics, resolve)
    })
  }
}
