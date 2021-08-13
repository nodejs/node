/** @type {import('unified').Plugin<[Options]|void[], Node, string>} */
export default function rehypeStringify(
  config: void | import('hast-util-to-html/lib/types').Options
): void
export type Root = import('hast').Root
export type Node = Root | Root['children'][number]
export type Options = import('hast-util-to-html').Options
