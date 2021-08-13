/**
 * @typedef {import('./core.js').Element} Element
 * @typedef {import('./core.js').Root} Root
 * @typedef {import('./core.js').HResult} HResult
 * @typedef {import('./core.js').HChild} HChild
 * @typedef {import('./core.js').HProperties} HProperties
 * @typedef {import('./core.js').HPropertyValue} HPropertyValue
 * @typedef {import('./core.js').HStyle} HStyle
 * @typedef {import('./core.js').core} Core
 *
 * @typedef {{[x: string]: HPropertyValue|HStyle|HChild}} JSXProps
 */
/**
 * @param {ReturnType<Core>} f
 */
export function runtime(f: ReturnType<Core>): {
  Fragment: null
  jsx: {
    (
      type: null | undefined,
      props: {
        children?: HChild
      },
      key?: string | undefined
    ): Root
    (type: string, props: JSXProps, key?: string | undefined): Element
  }
  jsxs: {
    (
      type: null | undefined,
      props: {
        children?: HChild
      },
      key?: string | undefined
    ): Root
    (type: string, props: JSXProps, key?: string | undefined): Element
  }
}
export type Element = import('./core.js').Element
export type Root = import('./core.js').Root
export type HResult = import('./core.js').HResult
export type HChild = import('./core.js').HChild
export type HProperties = import('./core.js').HProperties
export type HPropertyValue = import('./core.js').HPropertyValue
export type HStyle = import('./core.js').HStyle
export type Core = typeof import('./core.js').core
export type JSXProps = {
  [x: string]:
    | string
    | number
    | boolean
    | import('hast').Root
    | import('hast').Element
    | import('hast').DocType
    | import('hast').Comment
    | import('hast').Text
    | {
        [x: string]: import('./core.js').HStyleValue
      }
    | import('./core.js').HArrayValue
    | import('./core.js').HArrayChild
    | null
    | undefined
}
