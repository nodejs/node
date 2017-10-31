'use strict';

var supported = require('supports-color').hasBasic;
var width = require('string-width');
var stringify = require('unist-util-stringify-position');
var repeat = require('repeat-string');
var statistics = require('vfile-statistics');

module.exports = reporter;

/* Check which characters should be used. */
var windows = process.platform === 'win32';
/* istanbul ignore next - Windows. */
var chars = windows ? {error: '×', warning: '‼'} : {error: '✖', warning: '⚠'};

/* Match trailing white-space. */
var trailing = /\s*$/;

/* Default filename. */
var DEFAULT = '<stdin>';

var noop = {open: '', close: ''};

var colors = {
  underline: {open: '\u001b[4m', close: '\u001b[24m'},
  red: {open: '\u001b[31m', close: '\u001b[39m'},
  yellow: {open: '\u001b[33m', close: '\u001b[39m'},
  green: {open: '\u001b[32m', close: '\u001b[39m'}
};

var noops = {
  underline: noop,
  red: noop,
  yellow: noop,
  green: noop
};

/* Report a file’s messages. */
function reporter(files, options) {
  var settings = options || {};
  var one;

  if (!files) {
    return '';
  }

  /* Error. */
  if ('name' in files && 'message' in files) {
    return String(files.stack || files);
  }

  /* One file. */
  if (!('length' in files)) {
    one = true;
    files = [files];
  }

  return compile(parse(filter(files, settings), settings), one, settings);
}

function filter(files, options) {
  var result = [];
  var length = files.length;
  var index = -1;
  var file;

  if (!options.quiet && !options.silent) {
    return files.concat();
  }

  while (++index < length) {
    file = files[index];

    if (applicable(file, options).length !== 0) {
      result.push(file);
    }
  }

  return result;
}

function parse(files, options) {
  var length = files.length;
  var index = -1;
  var rows = [];
  var all = [];
  var locationSize = 0;
  var labelSize = 0;
  var reasonSize = 0;
  var ruleIdSize = 0;
  var file;
  var destination;
  var origin;
  var messages;
  var offset;
  var count;
  var message;
  var loc;
  var reason;
  var label;
  var id;

  while (++index < length) {
    file = files[index];
    destination = current(file);
    origin = file.history[0] || destination;
    messages = applicable(file, options).sort(comparator);

    if (rows.length !== 0 && rows[rows.length - 1].type !== 'header') {
      rows.push({type: 'separator'});
    }

    rows.push({
      type: 'header',
      origin: origin,
      destination: destination,
      name: origin || options.defaultName || DEFAULT,
      stored: Boolean(file.stored),
      moved: Boolean(file.stored && destination !== origin),
      stats: statistics(messages)
    });

    offset = -1;
    count = messages.length;

    while (++offset < count) {
      message = messages[offset];
      id = message.ruleId || '';
      reason = message.stack || message.message;
      loc = message.location;
      loc = stringify(loc.end.line && loc.end.column ? loc : loc.start);

      if (options.verbose && message.note) {
        reason += '\n' + message.note;
      }

      label = message.fatal ? 'error' : 'warning';

      rows.push({
        location: loc,
        label: label,
        reason: reason,
        ruleId: id,
        source: message.source
      });

      locationSize = Math.max(realLength(loc), locationSize);
      labelSize = Math.max(realLength(label), labelSize);
      reasonSize = Math.max(realLength(reason), reasonSize);
      ruleIdSize = Math.max(realLength(id), ruleIdSize);
    }

    all = all.concat(messages);
  }

  return {
    rows: rows,
    statistics: statistics(all),
    location: locationSize,
    label: labelSize,
    reason: reasonSize,
    ruleId: ruleIdSize
  };
}

function compile(map, one, options) {
  var enabled = options.color;
  var all = map.statistics;
  var rows = map.rows;
  var length = rows.length;
  var index = -1;
  var lines = [];
  var row;
  var line;
  var style;
  var color;

  if (enabled === null || enabled === undefined) {
    enabled = supported;
  }

  style = enabled ? colors : noops;

  while (++index < length) {
    row = rows[index];

    if (row.type === 'separator') {
      lines.push('');
    } else if (row.type === 'header') {
      if (one && !options.defaultName && !row.origin) {
        line = '';
      } else {
        color = style[row.stats.fatal ? 'red' : (row.stats.total ? 'yellow' : 'green')];
        line = style.underline.open + color.open + row.name + color.close + style.underline.close;
        line += row.moved ? ' > ' + row.destination : '';
      }

      if (!row.stats.total) {
        line += line ? ': ' : '';

        if (row.stored) {
          line += style.yellow.open + 'written' + style.yellow.close;
        } else {
          line += 'no issues found';
        }
      }

      if (line) {
        lines.push(line);
      }
    } else {
      color = style[row.label === 'error' ? 'red' : 'yellow'];

      lines.push([
        '',
        padLeft(row.location, map.location),
        padRight(color.open + row.label + color.close, map.label),
        padRight(row.reason, map.reason),
        padRight(row.ruleId, map.ruleId),
        row.source || ''
      ].join('  ').replace(trailing, ''));
    }
  }

  if (all.total) {
    line = [];

    if (all.fatal) {
      line.push([
        style.red.open + chars.error + style.red.close,
        all.fatal,
        all.fatal === 1 ? 'error' : 'errors'
      ].join(' '));
    }

    if (all.nonfatal) {
      line.push([
        style.yellow.open + chars.warning + style.yellow.close,
        all.nonfatal,
        all.nonfatal === 1 ? 'warning' : 'warnings'
      ].join(' '));
    }

    line = line.join(', ');

    if (all.fatal && all.nonfatal) {
      line = all.total + ' messages (' + line + ')';
    }

    lines.push('', line);
  }

  return lines.join('\n');
}

function applicable(file, options) {
  var messages = file.messages;
  var length = messages.length;
  var index = -1;
  var result = [];

  if (options.silent) {
    while (++index < length) {
      if (messages[index].fatal) {
        result.push(messages[index]);
      }
    }
  } else {
    result = messages.concat();
  }

  return result;
}

/* Get the length of `value`, ignoring ANSI sequences. */
function realLength(value) {
  var length = value.indexOf('\n');
  return width(length === -1 ? value : value.slice(0, length));
}

/* Pad `value` on the left. */
function padLeft(value, minimum) {
  return repeat(' ', minimum - realLength(value)) + value;
}

/* Pad `value` on the Right. */
function padRight(value, minimum) {
  return value + repeat(' ', minimum - realLength(value));
}

/* Comparator. */
function comparator(a, b) {
  return check(a, b, 'line') || check(a, b, 'column') || -1;
}

/* Compare a single property. */
function check(a, b, property) {
  return (a[property] || 0) - (b[property] || 0);
}

function current(file) {
  /* istanbul ignore if - Previous `vfile` version. */
  if (file.filePath) {
    return file.filePath();
  }

  return file.path;
}
