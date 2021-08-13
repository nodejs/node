/** @type {import('unified').Plugin<[Options?] | void[], string, Root>} */
export default function remarkParse(
  options: void | import('mdast-util-from-markdown/lib').Options | undefined
): void
export type Root = import('mdast').Root
export type Options = import('mdast-util-from-markdown').Options
