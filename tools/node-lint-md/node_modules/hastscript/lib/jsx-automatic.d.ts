import {HProperties, HChild, HResult} from './core.js'

export namespace JSX {
  /**
   * This defines the return value of JSX syntax.
   */
  type Element = HResult

  /**
   * This disallows the use of functional components.
   */
  type IntrinsicAttributes = never

  /**
   * This defines the prop types for known elements.
   *
   * For `hastscript` this defines any string may be used in combination with `hast` `Properties`.
   *
   * This **must** be an interface.
   */
  // eslint-disable-next-line @typescript-eslint/consistent-indexed-object-style
  interface IntrinsicElements {
    [name: string]:
      | HProperties
      | {
          /**
           * The prop that matches `ElementChildrenAttribute` key defines the type of JSX children, defines the children type.
           */
          children?: HChild
        }
  }

  /**
   * The key of this interface defines as what prop children are passed.
   */
  interface ElementChildrenAttribute {
    /**
     * Only the key matters, not the value.
     */
    children?: never
  }
}
