/**
 * @param {Options} [options]
 * @returns {MinimalDuplex}
 */
export function stream(
  options?: import('micromark-util-types').Options | undefined
): MinimalDuplex
export type Options = import('micromark-util-types').Options
export type Value = import('micromark-util-types').Value
export type Encoding = import('micromark-util-types').Encoding
export type Callback = (error?: Error | undefined) => void
export type MinimalDuplex = Omit<
  NodeJS.ReadableStream & NodeJS.WritableStream,
  | 'read'
  | 'setEncoding'
  | 'pause'
  | 'resume'
  | 'isPaused'
  | 'unpipe'
  | 'unshift'
  | 'wrap'
>
