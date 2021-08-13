export * from './jsx-automatic.js'
export const Fragment: null
export const jsx: {
  (
    type: null | undefined,
    props: {
      children?: import('./core.js').HChild
    },
    key?: string | undefined
  ): import('hast').Root
  (
    type: string,
    props: import('./runtime.js').JSXProps,
    key?: string | undefined
  ): import('hast').Element
}
export const jsxs: {
  (
    type: null | undefined,
    props: {
      children?: import('./core.js').HChild
    },
    key?: string | undefined
  ): import('hast').Root
  (
    type: string,
    props: import('./runtime.js').JSXProps,
    key?: string | undefined
  ): import('hast').Element
}
