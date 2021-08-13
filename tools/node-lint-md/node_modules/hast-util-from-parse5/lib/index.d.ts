/**
 * Transform Parse5’s AST to a hast tree.
 *
 * @param {P5Node} ast
 * @param {Options|VFile} [options]
 */
export function fromParse5(
  ast: P5Node,
  options?: import('vfile').VFile | Options | undefined
): Node
export type VFile = import('vfile').VFile
export type Schema = import('property-information').Schema
export type Position = import('unist').Position
export type Point = import('unist').Point
export type Parent = import('hast').Parent
export type Element = import('hast').Element
export type Root = import('hast').Root
export type Text = import('hast').Text
export type Comment = import('hast').Comment
export type Doctype = import('hast').DocType
export type Child = Parent['children'][number]
export type ElementChild = Element['children'][number]
export type Node = Child | Root
export type P5Document = import('parse5').Document
export type P5Doctype = import('parse5').DocumentType
export type P5Comment = import('parse5').CommentNode
export type P5Text = import('parse5').TextNode
export type P5Element = import('parse5').Element
export type P5ElementLocation = import('parse5').ElementLocation
export type P5Location = import('parse5').Location
export type P5Attribute = import('parse5').Attribute
export type P5Node = import('parse5').Node
export type Space = 'html' | 'svg'
export type Handler = (
  ctx: Context,
  node: P5Node,
  children?:
    | (
        | import('hast').Element
        | import('hast').Text
        | import('hast').Comment
        | import('hast').DocType
      )[]
    | undefined
) => Node
export type Options = {
  /**
   * Whether the root of the tree is in the `'html'` or `'svg'` space. If an element in with the SVG namespace is found in `ast`, `fromParse5` automatically switches to the SVG space when entering the element, and switches back when leaving
   */
  space?: Space | undefined
  /**
   * `VFile`, used to add positional information to nodes. If given, the file should have the original HTML source as its contents
   */
  file?: import('vfile').VFile | undefined
  /**
   * Whether to add extra positional information about starting tags, closing tags, and attributes to elements. Note: not used without `file`
   */
  verbose?: boolean | undefined
}
export type Context = {
  schema: Schema
  file: VFile | undefined
  verbose: boolean | undefined
  location: boolean
}
