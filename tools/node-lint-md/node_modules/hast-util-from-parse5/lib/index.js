/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('property-information').Schema} Schema
 * @typedef {import('unist').Position} Position
 * @typedef {import('unist').Point} Point
 * @typedef {import('hast').Parent} Parent
 * @typedef {import('hast').Element} Element
 * @typedef {import('hast').Root} Root
 * @typedef {import('hast').Text} Text
 * @typedef {import('hast').Comment} Comment
 * @typedef {import('hast').DocType} Doctype
 * @typedef {Parent['children'][number]} Child
 * @typedef {Element['children'][number]} ElementChild
 * @typedef {Child|Root} Node
 * @typedef {import('parse5').Document} P5Document
 * @typedef {import('parse5').DocumentType} P5Doctype
 * @typedef {import('parse5').CommentNode} P5Comment
 * @typedef {import('parse5').TextNode} P5Text
 * @typedef {import('parse5').Element} P5Element
 * @typedef {import('parse5').ElementLocation} P5ElementLocation
 * @typedef {import('parse5').Location} P5Location
 * @typedef {import('parse5').Attribute} P5Attribute
 * @typedef {import('parse5').Node} P5Node
 *
 * @typedef {'html'|'svg'} Space
 *
 * @callback Handler
 * @param {Context} ctx
 * @param {P5Node} node
 * @param {Array.<Child>} [children]
 * @returns {Node}
 *
 * @typedef Options
 * @property {Space} [space='html'] Whether the root of the tree is in the `'html'` or `'svg'` space. If an element in with the SVG namespace is found in `ast`, `fromParse5` automatically switches to the SVG space when entering the element, and switches back when leaving
 * @property {VFile} [file] `VFile`, used to add positional information to nodes. If given, the file should have the original HTML source as its contents
 * @property {boolean} [verbose=false] Whether to add extra positional information about starting tags, closing tags, and attributes to elements. Note: not used without `file`
 *
 * @typedef Context
 * @property {Schema} schema
 * @property {VFile|undefined} file
 * @property {boolean|undefined} verbose
 * @property {boolean} location
 */

import {h, s} from 'hastscript'
import {html, svg, find} from 'property-information'
import {location} from 'vfile-location'
import {webNamespaces} from 'web-namespaces'

const own = {}.hasOwnProperty

// Handlers.
const map = {
  '#document': root,
  '#document-fragment': root,
  '#text': text,
  '#comment': comment,
  '#documentType': doctype
}

/**
 * Transform Parse5’s AST to a hast tree.
 *
 * @param {P5Node} ast
 * @param {Options|VFile} [options]
 */
export function fromParse5(ast, options = {}) {
  /** @type {Options} */
  let settings
  /** @type {VFile|undefined} */
  let file

  if (isFile(options)) {
    file = options
    settings = {}
  } else {
    file = options.file
    settings = options
  }

  return transform(
    {
      schema: settings.space === 'svg' ? svg : html,
      file,
      verbose: settings.verbose,
      location: false
    },
    ast
  )
}

/**
 * Transform children.
 *
 * @param {Context} ctx
 * @param {P5Node} ast
 * @returns {Node}
 */
function transform(ctx, ast) {
  const schema = ctx.schema
  /** @type {Handler} */
  // @ts-expect-error: index is fine.
  const fn = own.call(map, ast.nodeName) ? map[ast.nodeName] : element
  /** @type {Array.<Child>|undefined} */
  let children

  // Element.
  if ('tagName' in ast) {
    ctx.schema = ast.namespaceURI === webNamespaces.svg ? svg : html
  }

  if ('childNodes' in ast) {
    children = nodes(ctx, ast.childNodes)
  }

  const result = fn(ctx, ast, children)

  if ('sourceCodeLocation' in ast && ast.sourceCodeLocation && ctx.file) {
    // @ts-expect-error It’s fine.
    const position = createLocation(ctx, result, ast.sourceCodeLocation)

    if (position) {
      ctx.location = true
      result.position = position
    }
  }

  ctx.schema = schema

  return result
}

/**
 * Transform children.
 *
 * @param {Context} ctx
 * @param {Array.<P5Node>} children
 * @returns {Array.<Child>}
 */
function nodes(ctx, children) {
  let index = -1
  /** @type {Array.<Child>} */
  const result = []

  while (++index < children.length) {
    // @ts-expect-error Assume no roots in children.
    result[index] = transform(ctx, children[index])
  }

  return result
}

/**
 * Transform a document.
 * Stores `ast.quirksMode` in `node.data.quirksMode`.
 *
 * @type {Handler}
 * @param {P5Document} ast
 * @param {Array.<Child>} children
 * @returns {Root}
 */
function root(ctx, ast, children) {
  /** @type {Root} */
  const result = {
    type: 'root',
    children,
    data: {quirksMode: ast.mode === 'quirks' || ast.mode === 'limited-quirks'}
  }

  if (ctx.file && ctx.location) {
    const doc = String(ctx.file)
    const loc = location(doc)
    result.position = {
      start: loc.toPoint(0),
      end: loc.toPoint(doc.length)
    }
  }

  return result
}

/**
 * Transform a doctype.
 *
 * @type {Handler}
 * @returns {Doctype}
 */
function doctype() {
  // @ts-expect-error Types are out of date.
  return {type: 'doctype'}
}

/**
 * Transform a text.
 *
 * @type {Handler}
 * @param {P5Text} ast
 * @returns {Text}
 */
function text(_, ast) {
  return {type: 'text', value: ast.value}
}

/**
 * Transform a comment.
 *
 * @type {Handler}
 * @param {P5Comment} ast
 * @returns {Comment}
 */
function comment(_, ast) {
  return {type: 'comment', value: ast.data}
}

/**
 * Transform an element.
 *
 * @type {Handler}
 * @param {P5Element} ast
 * @param {Array.<ElementChild>} children
 * @returns {Element}
 */
function element(ctx, ast, children) {
  const fn = ctx.schema.space === 'svg' ? s : h
  let index = -1
  /** @type {Object.<string, string>} */
  const props = {}

  while (++index < ast.attrs.length) {
    const attribute = ast.attrs[index]
    props[(attribute.prefix ? attribute.prefix + ':' : '') + attribute.name] =
      attribute.value
  }

  const result = fn(ast.tagName, props, children)

  if (result.tagName === 'template' && 'content' in ast) {
    const pos = ast.sourceCodeLocation
    const startTag = pos && pos.startTag && position(pos.startTag)
    const endTag = pos && pos.endTag && position(pos.endTag)

    /** @type {Root} */
    // @ts-expect-error Types are wrong.
    const content = transform(ctx, ast.content)

    if (startTag && endTag && ctx.file) {
      content.position = {start: startTag.end, end: endTag.start}
    }

    result.content = content
  }

  return result
}

/**
 * Create clean positional information.
 *
 * @param {Context} ctx
 * @param {Node} node
 * @param {P5ElementLocation} location
 * @returns {Position|null}
 */
function createLocation(ctx, node, location) {
  const result = position(location)

  if (node.type === 'element') {
    const tail = node.children[node.children.length - 1]

    // Bug for unclosed with children.
    // See: <https://github.com/inikulin/parse5/issues/109>.
    if (
      result &&
      !location.endTag &&
      tail &&
      tail.position &&
      tail.position.end
    ) {
      result.end = Object.assign({}, tail.position.end)
    }

    if (ctx.verbose) {
      /** @type {Object.<string, Position|null>} */
      const props = {}
      /** @type {string} */
      let key

      for (key in location.attrs) {
        if (own.call(location.attrs, key)) {
          props[find(ctx.schema, key).property] = position(location.attrs[key])
        }
      }

      node.data = {
        position: {
          opening: position(location.startTag),
          closing: location.endTag ? position(location.endTag) : null,
          properties: props
        }
      }
    }
  }

  return result
}

/**
 * @param {P5Location} loc
 * @returns {Position|null}
 */
function position(loc) {
  const start = point({
    line: loc.startLine,
    column: loc.startCol,
    offset: loc.startOffset
  })
  const end = point({
    line: loc.endLine,
    column: loc.endCol,
    offset: loc.endOffset
  })
  // @ts-expect-error `null` is fine.
  return start || end ? {start, end} : null
}

/**
 * @param {Point} point
 * @returns {Point|null}
 */
function point(point) {
  return point.line && point.column ? point : null
}

/**
 * @param {VFile|Options} value
 * @returns {value is VFile}
 */
function isFile(value) {
  return 'messages' in value
}
