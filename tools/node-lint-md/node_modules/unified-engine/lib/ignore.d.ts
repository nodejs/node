export class Ignore {
  /**
   * @param {Options} options
   */
  constructor(options: Options)
  /** @type {string} */
  cwd: string
  /** @type {ResolveFrom|undefined} */
  ignorePathResolveFrom: ResolveFrom | undefined
  /** @type {FindUp<IgnoreConfig>} */
  findUp: FindUp<IgnoreConfig>
  /**
   * @param {string} filePath
   * @param {Callback} callback
   */
  check(filePath: string, callback: Callback): void
}
export type IgnoreConfig = import('ignore').Ignore & {
  filePath: string
}
export type ResolveFrom = 'cwd' | 'dir'
export type Options = {
  cwd: string
  detectIgnore: boolean | undefined
  ignoreName: string | undefined
  ignorePath: string | undefined
  ignorePathResolveFrom: ResolveFrom | undefined
}
export type Callback = (
  error: Error | null,
  result?: boolean | undefined
) => any
import {FindUp} from './find-up.js'
