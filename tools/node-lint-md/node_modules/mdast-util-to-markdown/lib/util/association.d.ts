/**
 * The `label` of an association is the string value: character escapes and
 * references work, and casing is intact.
 * The `identifier` is used to match one association to another: controversially,
 * character escapes and references don’t work in this matching: `&copy;` does
 * not match `©`, and `\+` does not match `+`.
 * But casing is ignored (and whitespace) is trimmed and collapsed: ` A\nb`
 * matches `a b`.
 * So, we do prefer the label when figuring out how we’re going to serialize:
 * it has whitespace, casing, and we can ignore most useless character escapes
 * and all character references.
 *
 * @param {Association} node
 * @returns {string}
 */
export function association(node: Association): string
export type Association = import('mdast').Association
