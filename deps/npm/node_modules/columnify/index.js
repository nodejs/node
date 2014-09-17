"use strict"

var wcwidth = require('./width')
var utils = require('./utils')
var padRight = utils.padRight
var padCenter = utils.padCenter
var padLeft = utils.padLeft
var splitIntoLines = utils.splitIntoLines
var splitLongWords = utils.splitLongWords
var truncateString = utils.truncateString

var DEFAULTS = {
  maxWidth: Infinity,
  minWidth: 0,
  columnSplitter: ' ',
  truncate: false,
  truncateMarker: '…',
  preserveNewLines: false,
  paddingChr: ' ',
  showHeaders: true,
  headingTransform: function(key) {
    return key.toUpperCase()
  },
  dataTransform: function(cell, column, index) {
    return cell
  }
}

module.exports = function(items, options) {

  options = options || {}

  var columnConfigs = options.config || {}
  delete options.config // remove config so doesn't appear on every column.

  var maxLineWidth = options.maxLineWidth || Infinity
  delete options.maxLineWidth // this is a line control option, don't pass it to column

  // Option defaults inheritance:
  // options.config[columnName] => options => DEFAULTS
  options = mixin(options, DEFAULTS)
  options.config = options.config || Object.create(null)

  options.spacing = options.spacing || '\n' // probably useless
  options.preserveNewLines = !!options.preserveNewLines
  options.showHeaders = !!options.showHeaders;
  options.columns = options.columns || options.include // alias include/columns, prefer columns if supplied
  var columnNames = options.columns || [] // optional user-supplied columns to include

  items = toArray(items, columnNames)

  // if not suppled column names, automatically determine columns from data keys
  if (!columnNames.length) {
    items.forEach(function(item) {
      for (var columnName in item) {
        if (columnNames.indexOf(columnName) === -1) columnNames.push(columnName)
      }
    })
  }

  // initialize column defaults (each column inherits from options.config)
  var columns = columnNames.reduce(function(columns, columnName) {
    var column = Object.create(options)
    columns[columnName] = mixin(column, columnConfigs[columnName])
    return columns
  }, Object.create(null))

  // sanitize column settings
  columnNames.forEach(function(columnName) {
    var column = columns[columnName]
    column.maxWidth = Math.ceil(column.maxWidth)
    column.minWidth = Math.ceil(column.minWidth)
    column.truncate = !!column.truncate
  })

  // sanitize data
  items = items.map(function(item) {
    var result = Object.create(null)
    columnNames.forEach(function(columnName) {
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
  columnNames.forEach(function(columnName) {
    var column = columns[columnName]
    items = items.map(function(item, index) {
      item[columnName] = column.dataTransform(item[columnName], column, index)
      return item
    })
  })

  // add headers
  var headers = {}
  if(options.showHeaders) {
    columnNames.forEach(function(columnName) {
      var column = columns[columnName]
      headers[columnName] = column.headingTransform(columnName)
    })
    items.unshift(headers)
  }
  // get actual max-width between min & max
  // based on length of data in columns
  columnNames.forEach(function(columnName) {
    var column = columns[columnName]
    column.width = items.map(function(item) {
      return item[columnName]
    }).reduce(function(min, cur) {
      return Math.max(min, Math.min(column.maxWidth, Math.max(column.minWidth, wcwidth(cur))))
    }, 0)
  })

  // split long words so they can break onto multiple lines
  columnNames.forEach(function(columnName) {
    var column = columns[columnName]
    items = items.map(function(item) {
      item[columnName] = splitLongWords(item[columnName], column.width, column.truncateMarker)
      return item
    })
  })

  // wrap long lines. each item is now an array of lines.
  columnNames.forEach(function(columnName) {
    var column = columns[columnName]
    items = items.map(function(item, index) {
      var cell = item[columnName]
      item[columnName] = splitIntoLines(cell, column.width)

      // if truncating required, only include first line + add truncation char
      if (column.truncate && item[columnName].length > 1) {
          item[columnName] = splitIntoLines(cell, column.width - wcwidth(column.truncateMarker))
          var firstLine = item[columnName][0]
          if (!endsWith(firstLine, column.truncateMarker)) item[columnName][0] += column.truncateMarker
          item[columnName] = item[columnName].slice(0, 1)
      }
      return item
    })
  })

  // recalculate column widths from truncated output/lines
  columnNames.forEach(function(columnName) {
    var column = columns[columnName]
    column.width = items.map(function(item) {
      return item[columnName].reduce(function(min, cur) {
        return Math.max(min, Math.min(column.maxWidth, Math.max(column.minWidth, wcwidth(cur))))
      }, 0)
    }).reduce(function(min, cur) {
      return Math.max(min, Math.min(column.maxWidth, Math.max(column.minWidth, cur)))
    }, 0)
  })


  var rows = createRows(items, columns, columnNames, options.paddingChr) // merge lines into rows
  // conceive output
  return rows.reduce(function(output, row) {
    return output.concat(row.reduce(function(rowOut, line) {
      return rowOut.concat(line.join(options.columnSplitter))
    }, []))
  }, []).map(function(line) {
    return truncateString(line, maxLineWidth)
  }).join(options.spacing)
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
  return items.map(function(item) {
    var row = []
    var numLines = 0
    columnNames.forEach(function(columnName) {
      numLines = Math.max(numLines, item[columnName].length)
    })
    // combine matching lines of each rows
    for (var i = 0; i < numLines; i++) {
      row[i] = row[i] || []
      columnNames.forEach(function(columnName) {
        var column = columns[columnName]
        var val = item[columnName][i] || '' // || '' ensures empty columns get padded
        if (column.align == 'right') row[i].push(padLeft(val, column.width, paddingChr))
        else if (column.align == 'center') row[i].push(padCenter(val, column.width, paddingChr))
        else row[i].push(padRight(val, column.width, paddingChr))
      })
    }
    return row
  })
}

/**
 * Generic source->target mixin.
 * Copy properties from `source` into `target` if target doesn't have them.
 * Destructive. Modifies `target`.
 *
 * @param target Object target for mixin properties.
 * @param source Object source of mixin properties.
 * @return Object `target` after mixin applied.
 */

function mixin(target, source) {
  source = source || {}
  for (var key in source) {
    if (target.hasOwnProperty(key)) continue
    target[key] = source[key]
  }
  return target
}

/**
 * Adapted from String.prototype.endsWith polyfill.
 */

function endsWith(target, searchString, position) {
  position = position || target.length;
  position = position - searchString.length;
  var lastIndex = target.lastIndexOf(searchString);
  return lastIndex !== -1 && lastIndex === position;
}


function toArray(items, columnNames) {
  if (Array.isArray(items)) return items
  var rows = []
  for (var key in items) {
    var item = {}
    item[columnNames[0] || 'key'] = key
    item[columnNames[1] || 'value'] = items[key]
    rows.push(item)
  }
  return rows
}
