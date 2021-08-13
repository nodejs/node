/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('../configuration.js').Configuration} Configuration
 * @typedef {import('../index.js').Settings} Settings
 */

import {trough} from 'trough'
import {configure} from './configure.js'
import {fileSystem} from './file-system.js'
import {stdin} from './stdin.js'
import {transform} from './transform.js'
import {log} from './log.js'

export const fileSetPipeline = trough()
  .use(configure)
  .use(fileSystem)
  .use(stdin)
  .use(transform)
  .use(log)
