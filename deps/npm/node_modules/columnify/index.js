"use strict"

const wcwidth = require('./width')
const {
  padRight,
  padCenter,
  padLeft,
  splitIntoLines,
  splitLongWords,
  truncateString
} = require('./utils')

const DEFAULT_HEADING_TRANSFORM = key => key.toUpperCase()

const DEFAULT_DATA_TRANSFORM = (cell, column, index) => cell

const DEFAULTS = Object.freeze({
  maxWidth: Infinity,
  minWidth: 0,
  columnSplitter: ' ',
  truncate: false,
  truncateMarker: '…',
  preserveNewLines: false,
  paddingChr: ' ',
  showHeaders: true,
  headingTransform: DEFAULT_HEADING_TRANSFORM,
  dataTransform: DEFAULT_DATA_TRANSFORM
})

module.exports = function(items, options = {}) {

  let columnConfigs = options.config || {}
  delete options.config // remove config so doesn't appear on every column.

  let maxLineWidth = options.maxLineWidth || Infinity
  if (maxLineWidth === 'auto') maxLineWidth = process.stdout.columns || Infinity
  delete options.maxLineWidth // this is a line control option, don't pass it to column

  // Option defaults inheritance:
  // options.config[columnName] => options => DEFAULTS
  options = mixin({}, DEFAULTS, options)

  options.config = options.config || Object.create(null)

  options.spacing = options.spacing || '\n' // probably useless
  options.preserveNewLines = !!options.preserveNewLines
  options.showHeaders = !!options.showHeaders;
  options.columns = options.columns || options.include // alias include/columns, prefer columns if supplied
  let columnNames = options.columns || [] // optional user-supplied columns to include

  items = toArray(items, columnNames)

  // if not suppled column names, automatically determine columns from data keys
  if (!columnNames.length) {
    items.forEach(function(item) {
      for (let columnName in item) {
        if (columnNames.indexOf(columnName) === -1) columnNames.push(columnName)
      }
    })
  }

  // initialize column defaults (each column inherits from options.config)
  let columns = columnNames.reduce((columns, columnName) => {
    let column = Object.create(options)
    columns[columnName] = mixin(column, columnConfigs[columnName])
    return columns
  }, Object.create(null))

  // sanitize column settings
  columnNames.forEach(columnName => {
    let column = columns[columnName]
    column.name = columnName
    column.maxWidth = Math.ceil(column.maxWidth)
    column.minWidth = Math.ceil(column.minWidth)
    column.truncate = !!column.truncate
    column.align = column.align || 'left'
  })

  // sanitize data
  items = items.map(item => {
    let result = Object.create(null)
    columnNames.forEach(columnName => {
      // null/undefined -> ''
      result[columnName] = item[columnName] != null ? item[columnName] : ''
      // toString everything
      result[columnName] = '' + result[columnName]
      if (columns[columnName].preserveNewLines) {
        // merge non-newline whitespace chars
        result[columnName] = result[columnName].replace(/[^\S\n]/gmi, ' ')
      } else {
        // merge all whitespace chars
        result[columnName] = result[columnName].replace(/\s/gmi, ' ')
      }
    })
    return result
  })

  // transform data cells
  columnNames.forEach(columnName => {
    let column = columns[columnName]
    items = items.map((item, index) => {
      let col = Object.create(column)
      item[columnName] = column.dataTransform(item[columnName], col, index)

      let changedKeys = Object.keys(col)
      // disable default heading transform if we wrote to column.name
      if (changedKeys.indexOf('name') !== -1) {
        if (column.headingTransform !== DEFAULT_HEADING_TRANSFORM) return
        column.headingTransform = heading => heading
      }
      changedKeys.forEach(key => column[key] = col[key])
      return item
    })
  })

  // add headers
  let headers = {}
  if(options.showHeaders) {
    columnNames.forEach(columnName => {
      let column = columns[columnName]
      headers[columnName] = column.headingTransform(column.name)
    })
    items.unshift(headers)
  }
  // get actual max-width between min & max
  // based on length of data in columns
  columnNames.forEach(columnName => {
    let column = columns[columnName]
    column.width = items
    .map(item => item[columnName])
    .reduce((min, cur) => {
      return Math.max(min, Math.min(column.maxWidth, Math.max(column.minWidth, wcwidth(cur))))
    }, 0)
  })

  // split long words so they can break onto multiple lines
  columnNames.forEach(columnName => {
    let column = columns[columnName]
    items = items.map(item => {
      item[columnName] = splitLongWords(item[columnName], column.width, column.truncateMarker)
      return item
    })
  })

  // wrap long lines. each item is now an array of lines.
  columnNames.forEach(columnName => {
    let column = columns[columnName]
    items = items.map((item, index) => {
      let cell = item[columnName]
      item[columnName] = splitIntoLines(cell, column.width)

      // if truncating required, only include first line + add truncation char
      if (column.truncate && item[columnName].length > 1) {
        item[columnName] = splitIntoLines(cell, column.width - wcwidth(column.truncateMarker))
        let firstLine = item[columnName][0]
        if (!endsWith(firstLine, column.truncateMarker)) item[columnName][0] += column.truncateMarker
        item[columnName] = item[columnName].slice(0, 1)
      }
      return item
    })
  })

  // recalculate column widths from truncated output/lines
  columnNames.forEach(columnName => {
    let column = columns[columnName]
    column.width = items.map(item => {
      return item[columnName].reduce((min, cur) => {
        return Math.max(min, Math.min(column.maxWidth, Math.max(column.minWidth, wcwidth(cur))))
      }, 0)
    }).reduce((min, cur) => {
      return Math.max(min, Math.min(column.maxWidth, Math.max(column.minWidth, cur)))
    }, 0)
  })


  let rows = createRows(items, columns, columnNames, options.paddingChr) // merge lines into rows
  // conceive output
  return rows.reduce((output, row) => {
    return output.concat(row.reduce((rowOut, line) => {
      return rowOut.concat(line.join(options.columnSplitter))
    }, []))
  }, [])
  .map(line => truncateString(line, maxLineWidth))
  .join(options.spacing)
}

/**
 * Convert wrapped lines into rows with padded values.
 *
 * @param Array items data to process
 * @param Array columns column width settings for wrapping
 * @param Array columnNames column ordering
 * @return Array items wrapped in arrays, corresponding to lines
 */

function createRows(items, columns, columnNames, paddingChr) {
  return items.map(item => {
    let row = []
    let numLines = 0
    columnNames.forEach(columnName => {
      numLines = Math.max(numLines, item[columnName].length)
    })
    // combine matching lines of each rows
    for (let i = 0; i < numLines; i++) {
      row[i] = row[i] || []
      columnNames.forEach(columnName => {
        let column = columns[columnName]
        let val = item[columnName][i] || '' // || '' ensures empty columns get padded
        if (column.align === 'right') row[i].push(padLeft(val, column.width, paddingChr))
        else if (column.align === 'center' || column.align === 'centre') row[i].push(padCenter(val, column.width, paddingChr))
        else row[i].push(padRight(val, column.width, paddingChr))
      })
    }
    return row
  })
}

/**
 * Object.assign
 *
 * @return Object Object with properties mixed in.
 */

function mixin(...args) {
  if (Object.assign) return Object.assign(...args)
  return ObjectAssign(...args)
}

function ObjectAssign(target, firstSource) {
  "use strict";
  if (target === undefined || target === null)
    throw new TypeError("Cannot convert first argument to object");

  var to = Object(target);

  var hasPendingException = false;
  var pendingException;

  for (var i = 1; i < arguments.length; i++) {
    var nextSource = arguments[i];
    if (nextSource === undefined || nextSource === null)
      continue;

    var keysArray = Object.keys(Object(nextSource));
    for (var nextIndex = 0, len = keysArray.length; nextIndex < len; nextIndex++) {
      var nextKey = keysArray[nextIndex];
      try {
        var desc = Object.getOwnPropertyDescriptor(nextSource, nextKey);
        if (desc !== undefined && desc.enumerable)
          to[nextKey] = nextSource[nextKey];
      } catch (e) {
        if (!hasPendingException) {
          hasPendingException = true;
          pendingException = e;
        }
      }
    }

    if (hasPendingException)
      throw pendingException;
  }
  return to;
}

/**
 * Adapted from String.prototype.endsWith polyfill.
 */

function endsWith(target, searchString, position) {
  position = position || target.length;
  position = position - searchString.length;
  let lastIndex = target.lastIndexOf(searchString);
  return lastIndex !== -1 && lastIndex === position;
}


function toArray(items, columnNames) {
  if (Array.isArray(items)) return items
  let rows = []
  for (let key in items) {
    let item = {}
    item[columnNames[0] || 'key'] = key
    item[columnNames[1] || 'value'] = items[key]
    rows.push(item)
  }
  return rows
}
