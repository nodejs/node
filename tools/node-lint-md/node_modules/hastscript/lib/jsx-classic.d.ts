import {HProperties, HChild, HResult} from './core.js'

/**
 * This unique symbol is declared to specify the key on which JSX children are passed, without conflicting
 * with the Attributes type.
 */
declare const children: unique symbol

/**
 * This defines the return value of JSX syntax.
 */
export type Element = HResult

/**
 * This disallows the use of functional components.
 */
export type IntrinsicAttributes = never

/**
 * This defines the prop types for known elements.
 *
 * For `hastscript` this defines any string may be used in combination with `hast` `Properties`.
 *
 * This **must** be an interface.
 */
// eslint-disable-next-line @typescript-eslint/consistent-indexed-object-style
export interface IntrinsicElements {
  [name: string]:
    | HProperties
    | {
        /**
         * The prop that matches `ElementChildrenAttribute` key defines the type of JSX children, defines the children type.
         */
        [children]?: HChild
      }
}

/**
 * The key of this interface defines as what prop children are passed.
 */
export interface ElementChildrenAttribute {
  /**
   * Only the key matters, not the value.
   */
  [children]?: never
}
