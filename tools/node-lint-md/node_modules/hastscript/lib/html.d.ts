export const h: {
  (): import('hast').Root
  (
    selector: null | undefined,
    ...children: import('./core.js').HChild[]
  ): import('hast').Root
  (
    selector: string,
    properties?: import('./core.js').HProperties | undefined,
    ...children: import('./core.js').HChild[]
  ): import('hast').Element
  (
    selector: string,
    ...children: import('./core.js').HChild[]
  ): import('hast').Element
}
export namespace h {
  namespace JSX {
    type Element = import('./jsx-classic').Element
    type IntrinsicAttributes = import('./jsx-classic').IntrinsicAttributes
    type IntrinsicElements = import('./jsx-classic').IntrinsicElements
    type ElementChildrenAttribute =
      import('./jsx-classic').ElementChildrenAttribute
  }
}
/**
 * Acceptable child value
 */
export type Child = import('./core.js').HChild
/**
 * Acceptable properties value.
 */
export type Properties = import('./core.js').HProperties
