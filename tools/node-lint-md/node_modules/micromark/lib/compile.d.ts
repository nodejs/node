/**
 * @param {CompileOptions} [options]
 * @returns {Compile}
 */
export function compile(
  options?: import('micromark-util-types').CompileOptions | undefined
): Compile
export type Event = import('micromark-util-types').Event
export type CompileOptions = import('micromark-util-types').CompileOptions
export type CompileData = import('micromark-util-types').CompileData
export type CompileContext = import('micromark-util-types').CompileContext
export type Compile = import('micromark-util-types').Compile
export type Handle = import('micromark-util-types').Handle
export type HtmlExtension = import('micromark-util-types').HtmlExtension
export type NormalizedHtmlExtension =
  import('micromark-util-types').NormalizedHtmlExtension
export type Media = {
  image?: boolean | undefined
  labelId?: string | undefined
  label?: string | undefined
  referenceId?: string | undefined
  destination?: string | undefined
  title?: string | undefined
}
export type Definition = {
  destination?: string | undefined
  title?: string | undefined
}
