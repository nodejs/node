export type UnistParent = import('unist').Parent
export type Root = import('mdast').Root
export type Content = import('mdast').Content
export type Node = Root | Content
export type Parent = Extract<Node, UnistParent>
export type SafeOptions = {
  before: string
  after: string
}
export type Enter = (type: string) => Exit
export type Exit = () => void
export type Context = {
  /**
   *   Stack of labels.
   */
  stack: string[]
  /**
   *   Positions of children in their parents.
   */
  indexStack: number[]
  enter: Enter
  options: Options
  unsafe: Array<Unsafe>
  join: Array<Join>
  handle: Handle
  handlers: Handlers
  /**
   *   The marker used by the current list.
   */
  bulletCurrent: string | undefined
  /**
   *   The marker used by the previous list.
   */
  bulletLastUsed: string | undefined
}
export type Handle = (
  node: any,
  parent: Parent | null | undefined,
  context: Context,
  safeOptions: SafeOptions
) => string
export type Handlers = Record<string, Handle>
export type Join = (
  left: Node,
  right: Node,
  parent: Parent,
  context: Context
) => boolean | null | void | number
export type Unsafe = {
  character: string
  inConstruct?: string | string[] | undefined
  notInConstruct?: string | string[] | undefined
  after?: string | undefined
  before?: string | undefined
  atBreak?: boolean | undefined
  /**
   * The unsafe pattern compiled as a regex
   */
  _compiled?: RegExp | undefined
}
export type Options = {
  bullet?: '-' | '*' | '+' | undefined
  bulletOther?: '-' | '*' | '+' | undefined
  bulletOrdered?: '.' | ')' | undefined
  bulletOrderedOther?: '.' | ')' | undefined
  closeAtx?: boolean | undefined
  emphasis?: '*' | '_' | undefined
  fence?: '~' | '`' | undefined
  fences?: boolean | undefined
  incrementListMarker?: boolean | undefined
  listItemIndent?: 'tab' | 'one' | 'mixed' | undefined
  quote?: '"' | "'" | undefined
  resourceLink?: boolean | undefined
  rule?: '-' | '*' | '_' | undefined
  ruleRepetition?: number | undefined
  ruleSpaces?: boolean | undefined
  setext?: boolean | undefined
  strong?: '*' | '_' | undefined
  tightDefinitions?: boolean | undefined
  extensions?: Options[] | undefined
  handlers?: Handlers | undefined
  join?: Join[] | undefined
  unsafe?: Unsafe[] | undefined
}
