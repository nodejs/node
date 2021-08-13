import {color} from './color.js'

/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Position} Position
 * @typedef {import('unist').Point} Point
 *
 * @typedef {Object} InspectOptions
 * @property {boolean} [showPositions=true]
 */

/* c8 ignore next */
export var inspect = color ? inspectColor : inspectNoColor

var own = {}.hasOwnProperty

var bold = ansiColor(1, 22)
var dim = ansiColor(2, 22)
var yellow = ansiColor(33, 39)
var green = ansiColor(32, 39)

// ANSI color regex.
/* eslint-disable-next-line no-control-regex */
var colorExpression = /(?:(?:\u001B\[)|\u009B)(?:\d{1,3})?(?:(?:;\d{0,3})*)?[A-M|f-m]|\u001B[A-M]/g

/**
 * Inspects a node, without using color.
 *
 * @param {unknown} node
 * @param {InspectOptions} [options]
 * @returns {string}
 */
export function inspectNoColor(node, options) {
  return inspectColor(node, options).replace(colorExpression, '')
}

/**
 * Inspects a node, using color.
 *
 * @param {unknown} tree
 * @param {InspectOptions} [options]
 * @returns {string}
 */
export function inspectColor(tree, options) {
  var positions =
    !options ||
    options.showPositions === null ||
    options.showPositions === undefined
      ? true
      : options.showPositions

  return inspectValue(tree)

  /**
   * @param {unknown} node
   * @returns {string}
   */
  function inspectValue(node) {
    if (node && typeof node === 'object' && 'length' in node) {
      // @ts-ignore looks like a list of nodes.
      return inspectNodes(node)
    }

    // @ts-ignore looks like a single node.
    if (node && node.type) {
      // @ts-ignore looks like a single node.
      return inspectTree(node)
    }

    return inspectNonTree(node)
  }

  /**
   * @param {unknown} value
   * @returns {string}
   */
  function inspectNonTree(value) {
    return JSON.stringify(value)
  }

  /**
   * @param {Node[]} nodes
   * @returns {string}
   */
  function inspectNodes(nodes) {
    /** @type {Array.<string>} */
    var result = []
    var index = -1

    while (++index < nodes.length) {
      result.push(
        dim((index < nodes.length - 1 ? '├' : '└') + '─' + index) +
          ' ' +
          indent(
            inspectValue(nodes[index]),
            (index < nodes.length - 1 ? dim('│') : ' ') + '   ',
            true
          )
      )
    }

    return result.join('\n')
  }

  /**
   * @param {Object.<string, unknown>} object
   * @returns {string}
   */
  function inspectFields(object) {
    /** @type {Array.<string>} */
    var result = []
    /** @type {string} */
    var key
    /** @type {unknown} */
    var value
    /** @type {string} */
    var formatted

    for (key in object) {
      /* c8 ignore next 1 */
      if (!own.call(object, key)) continue

      value = object[key]

      if (
        value === undefined ||
        // Standard keys defined by unist that we format differently.
        // <https://github.com/syntax-tree/unist>
        key === 'type' ||
        key === 'value' ||
        key === 'children' ||
        key === 'position' ||
        // Ignore `name` (from xast) and `tagName` (from `hast`) when string.
        (typeof value === 'string' && (key === 'name' || key === 'tagName'))
      ) {
        continue
      }

      // A single node.
      if (
        value &&
        typeof value === 'object' &&
        // @ts-ignore looks like a node.
        value.type &&
        key !== 'data' &&
        key !== 'attributes' &&
        key !== 'properties'
      ) {
        // @ts-ignore looks like a node.
        formatted = inspectTree(value)
      }
      // A list of nodes.
      else if (
        value &&
        typeof value === 'object' &&
        'length' in value &&
        value[0] &&
        value[0].type
      ) {
        // @ts-ignore looks like a list of nodes.
        formatted = '\n' + inspectNodes(value)
      } else {
        formatted = inspectNonTree(value)
      }

      result.push(
        key + dim(':') + (/\s/.test(formatted.charAt(0)) ? '' : ' ') + formatted
      )
    }

    return indent(
      result.join('\n'),
      // @ts-ignore looks like a parent node.
      (object.children && object.children.length > 0 ? dim('│') : ' ') + ' '
    )
  }

  /**
   * @param {Node} node
   * @returns {string}
   */
  function inspectTree(node) {
    var result = [formatNode(node)]
    var fields = inspectFields(node)
    // @ts-ignore looks like a parent.
    var content = inspectNodes(node.children || [])
    if (fields) result.push(fields)
    if (content) result.push(content)
    return result.join('\n')
  }

  /**
   * Colored node formatter.
   *
   * @param {Node} node
   * @returns {string}
   */
  function formatNode(node) {
    var result = [bold(node.type)]
    var kind = node.tagName || node.name
    var position = positions ? stringifyPosition(node.position) : ''

    if (typeof kind === 'string') {
      result.push('<', kind, '>')
    }

    if (node.children) {
      // @ts-ignore looks like a parent.
      result.push(dim('['), yellow(node.children.length), dim(']'))
    } else if (typeof node.value === 'string') {
      result.push(' ', green(inspectNonTree(node.value)))
    }

    if (position) {
      result.push(' ', dim('('), position, dim(')'))
    }

    return result.join('')
  }
}

/**
 * @param {string} value
 * @param {string} indentation
 * @param {boolean} [ignoreFirst=false]
 * @returns {string}
 */
function indent(value, indentation, ignoreFirst) {
  var lines = value.split('\n')
  var index = ignoreFirst ? 0 : -1

  if (!value) return value

  while (++index < lines.length) {
    lines[index] = indentation + lines[index]
  }

  return lines.join('\n')
}

/**
 * @param {Position} value
 * @returns {string}
 */
function stringifyPosition(value) {
  /** @type {Position} */
  // @ts-ignore
  var position = value || {}
  /** @type {Array.<string>} */
  var result = []
  /** @type {Array.<string>} */
  var positions = []
  /** @type {Array.<string>} */
  var offsets = []

  point(position.start)
  point(position.end)

  if (positions.length > 0) result.push(positions.join('-'))
  if (offsets.length > 0) result.push(offsets.join('-'))

  return result.join(', ')

  /**
   * @param {Point} value
   */
  function point(value) {
    if (value) {
      positions.push((value.line || 1) + ':' + (value.column || 1))

      if ('offset' in value) {
        offsets.push(String(value.offset || 0))
      }
    }
  }
}

/**
 * Factory to wrap values in ANSI colours.
 *
 * @param {number} open
 * @param {number} close
 * @returns {function(string): string}
 */
function ansiColor(open, close) {
  return color

  /**
   * @param {string} value
   * @returns {string}
   */
  function color(value) {
    return '\u001B[' + open + 'm' + value + '\u001B[' + close + 'm'
  }
}
