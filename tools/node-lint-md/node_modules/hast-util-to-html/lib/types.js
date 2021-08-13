/**
 * @typedef {import('hast').Parent} Parent
 * @typedef {import('hast').Literal} Literal
 * @typedef {import('hast').Root} Root
 * @typedef {import('hast').Comment} Comment
 * @typedef {import('hast').Element} Element
 * @typedef {import('hast').Text} Text
 * @typedef {Literal & {type: 'raw'}} Raw
 * @typedef {Parent['children'][number]} Child
 * @typedef {import('hast').Properties} Properties
 * @typedef {Properties[string]} PropertyValue
 * @typedef {Child|Root} Node
 *
 * @typedef {import('stringify-entities').StringifyEntitiesOptions} StringifyEntitiesOptions
 * @typedef {import('property-information').Schema} Schema
 *
 * @callback Handle
 * @param {Context} context
 * @param {Node} node
 * @param {number|null} index
 * @param {Parent|null} parent
 * @returns {string}
 *
 * @callback OmitHandle
 * @param {Element} node
 * @param {number|null} index
 * @param {Parent|null} parent
 * @returns {boolean}
 *
 * @typedef {{opening?: OmitHandle, closing?: OmitHandle}} Omission
 *
 * @typedef {'html'|'svg'} Space
 * @typedef {'"'|"'"} Quote
 *
 * @typedef Options
 * @property {Space} [space='html'] Whether the *root* of the *tree* is in the `'html'` or `'svg'` space. If an `svg` element is found in the HTML space, `toHtml` automatically switches to the SVG space when entering the element, and switches back when exiting
 * @property {Omit<StringifyEntitiesOptions, 'escapeOnly'|'attribute'|'subset'>} [entities] Configuration for `stringify-entities`
 * @property {Array<string>} [voids] Tag names of *elements* to serialize without closing tag. Not used in the SVG space. Defaults to `html-void-elements`
 * @property {boolean} [upperDoctype=false] Use a `<!DOCTYPE…` instead of `<!doctype…`. Useless except for XHTML
 * @property {Quote} [quote='"'] Preferred quote to use
 * @property {boolean} [quoteSmart=false] Use the other quote if that results in less bytes
 * @property {boolean} [preferUnquoted=false] Leave attributes unquoted if that results in less bytes. Not used in the SVG space
 * @property {boolean} [omitOptionalTags=false] Omit optional opening and closing tags. For example, in `<ol><li>one</li><li>two</li></ol>`, both `</li>` closing tags can be omitted. The first because it’s followed by another `li`, the last because it’s followed by nothing. Not used in the SVG space
 * @property {boolean} [collapseEmptyAttributes=false] Collapse empty attributes: `class=""` is stringified as `class` instead. **Note**: boolean attributes, such as `hidden`, are always collapsed. Not used in the SVG space
 * @property {boolean} [closeSelfClosing=false] Close self-closing nodes with an extra slash (`/`): `<img />` instead of `<img>`. See `tightSelfClosing` to control whether a space is used before the slash. Not used in the SVG space
 * @property {boolean} [closeEmptyElements=false] Close SVG elements without any content with slash (`/`) on the opening tag instead of an end tag: `<circle />` instead of `<circle></circle>`. See `tightSelfClosing` to control whether a space is used before the slash. Not used in the HTML space
 * @property {boolean} [tightSelfClosing=false] Do not use an extra space when closing self-closing elements: `<img/>` instead of `<img />`. **Note**: Only used if `closeSelfClosing: true` or `closeEmptyElements: true`
 * @property {boolean} [tightCommaSeparatedLists=false] Join known comma-separated attribute values with just a comma (`,`), instead of padding them on the right as well (`,·`, where `·` represents a space)
 * @property {boolean} [tightAttributes=false] Join attributes together, without whitespace, if possible: `class="a b" title="c d"` is stringified as `class="a b"title="c d"` instead to save bytes. **Note**: creates invalid (but working) markup. Not used in the SVG space
 * @property {boolean} [tightDoctype=false] Drop unneeded spaces in doctypes: `<!doctypehtml>` instead of `<!doctype html>` to save bytes. **Note**: creates invalid (but working) markup
 * @property {boolean} [bogusComments=false] Use “bogus comments” instead of comments to save byes: `<?charlie>` instead of `<!--charlie-->`. **Note**: creates invalid (but working) markup
 * @property {boolean} [allowParseErrors=false] Do not encode characters which cause parse errors (even though they work), to save bytes. **Note**: creates invalid (but working) markup. Not used in the SVG space
 * @property {boolean} [allowDangerousCharacters=false] Do not encode some characters which cause XSS vulnerabilities in older browsers. **Note**: Only set this if you completely trust the content
 * @property {boolean} [allowDangerousHtml=false] Allow `raw` nodes and insert them as raw HTML. When falsey, encodes `raw` nodes. **Note**: Only set this if you completely trust the content
 *
 * @typedef Context
 * @property {number} valid
 * @property {number} safe
 * @property {Schema} schema
 * @property {Omission} omit
 * @property {Quote} quote
 * @property {Quote} alternative
 * @property {boolean} smart
 * @property {boolean} unquoted
 * @property {boolean} tight
 * @property {boolean} upperDoctype
 * @property {boolean} tightDoctype
 * @property {boolean} bogusComments
 * @property {boolean} tightLists
 * @property {boolean} tightClose
 * @property {boolean} collapseEmpty
 * @property {boolean} dangerous
 * @property {Array.<string>} voids
 * @property {StringifyEntitiesOptions} entities
 * @property {boolean} close
 * @property {boolean} closeEmpty
 */

export {}
