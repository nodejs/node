/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module remark:message-control
 * @fileoverview Enable, disable, and ignore messages with remark.
 */

'use strict';

/* Dependencies. */
var trim = require('trim');
var vfileLocation = require('vfile-location');
var visit = require('unist-util-visit');
var marker = require('mdast-comment-marker');

/* Map of allowed verbs. */
var ALLOWED_VERBS = {
  enable: true,
  disable: true,
  ignore: true
};

/* Expose. */
module.exports = attacher;

/* Filter .*/
function attacher(processor, options) {
  var name = options && options.name;
  var sources;
  var known;
  var reset;
  var enable;
  var disable;

  if (!name) {
    throw new Error('Expected `name` in `options`, got `' + name + '`');
  }

  known = options.known;
  reset = options.reset;
  enable = options.enable || [];
  disable = options.disable || [];
  sources = options.source;

  if (!sources) {
    sources = [name];
  } else if (typeof sources === 'string') {
    sources = [sources];
  }

  return transformer;

  function transformer(ast, file) {
    var location = vfileLocation(file);
    var initial = !reset;
    var gaps = detectGaps(ast, file);
    var scope = {};
    var globals = [];

    /**
     * Helper to check (and possibly warn) if a ruleId
     * is unknown.
     *
     * @param {string} ruleId - Identifier.
     * @param {string} verb - Thing to do with `ruleId`.
     * @param {Position} pos - Position of marker.
     */
    function isKnown(ruleId, verb, pos) {
      var result = known ? known.indexOf(ruleId) !== -1 : true;

      if (!result) {
        file.warn('Unknown rule: cannot ' + verb + ' `\'' + ruleId + '\'`', pos);
      }

      return result;
    }

    /* Get the latest state of a rule.
     * When without `ruleId`, gets global state. */
    function getState(ruleId) {
      var ranges = ruleId ? scope[ruleId] : globals;

      if (ranges && ranges.length) {
        return ranges[ranges.length - 1].state;
      }

      if (!ruleId) {
        return !reset;
      }

      if (reset) {
        return enable.indexOf(ruleId) !== -1;
      }

      return disable.indexOf(ruleId) === -1;
    }

    /* Handle a rule. */
    function toggle(pos, state, ruleId) {
      var markers = ruleId ? scope[ruleId] : globals;
      var currentState;
      var previousState;

      if (!markers) {
        markers = scope[ruleId] = [];
      }

      previousState = getState(ruleId);
      currentState = state;

      if (currentState !== previousState) {
        markers.push({state: currentState, position: pos});
      }

      /* Toggle all known rules. */
      if (!ruleId) {
        for (ruleId in scope) {
          toggle(pos, state, ruleId);
        }
      }
    }

    visit(ast, 'html', function (node, position, parent) {
      var mark = marker(node);
      var ruleIds;
      var ruleId;
      var verb;
      var index;
      var length;
      var next;
      var pos;
      var tail;

      if (!mark || mark.name !== options.name) {
        return;
      }

      ruleIds = mark.attributes.split(/\s/g);
      verb = ruleIds.shift();
      next = parent.children[position + 1];
      pos = mark.node.position && mark.node.position.start;
      tail = next && next.position && next.position.end;

      if (!verb || !ALLOWED_VERBS[verb] === true) {
        return file.fail(
          'Unknown keyword `' + verb + '`: expected ' +
          '`\'enable\'`, `\'disable\'`, or `\'ignore\'`',
          mark.node
        );
      }

      length = ruleIds.length;
      index = -1;

      while (++index < length) {
        ruleId = ruleIds[index];

        if (isKnown(ruleId, verb, mark.node)) {
          toggle(pos, verb === 'enable', ruleId);

          if (verb === 'ignore') {
            toggle(tail, true, ruleId);
          }
        }
      }

      /* Apply to all rules. */
      if (!length) {
        if (verb === 'ignore') {
          toggle(pos, false);
          toggle(tail, true);
        } else {
          toggle(pos, verb === 'enable');
          reset = verb !== 'enable';
        }
      }
    });

    /* Check all `ranges` for `message`. */
    function check(message, ranges, id) {
      /* Check the state at the message's position. */
      var index = ranges && ranges.length;
      var length = -1;
      var range;

      while (--index > length) {
        range = ranges[index];

        /* istanbul ignore if - generated marker. */
        if (
          !range.position ||
          !range.position.line ||
          !range.position.column
        ) {
          continue;
        }

        if (
          range.position.line < message.line ||
          (
            range.position.line === message.line &&
            range.position.column < message.column
          )
        ) {
          return range.state === true;
        }
      }

      /* The first marker ocurred after the first
       * message, so we check the initial state. */
      if (!id) {
        return initial || reset;
      }

      return reset ? enable.indexOf(id) !== -1 : disable.indexOf(id) === -1;
    }

    file.messages = file.messages.filter(function (message) {
      var gapIndex = gaps.length;
      var ruleId = message.ruleId;
      var ranges = scope[ruleId];
      var pos;

      /* Keep messages from a different source. */
      if (!message.source || sources.indexOf(message.source) === -1) {
        return true;
      }

      /* We only ignore messages if they‘re disabled,
       * *not* when they’re not in the document. */
      if (!message.line) {
        message.line = 1;
      }

      if (!message.column) {
        message.column = 1;
      }

      /* Check whether the warning is inside a gap. */
      pos = location.toOffset(message);

      while (gapIndex--) {
        if (
          gaps[gapIndex].start <= pos &&
          gaps[gapIndex].end > pos
        ) {
          return false;
        }
      }

      /* Check whether allowed by specific and global states. */
      return check(message, ranges, ruleId) && check(message, globals);
    });
  }
}

/**
 * Detect gaps in `ast`.
 *
 * @param {Node} ast - Syntax tree.
 * @param {VFile} file - Virtual file.
 * @return {Array.<Object>} - Gaps.
 */
function detectGaps(ast, file) {
  var lastNode = ast.children[ast.children.length - 1];
  var offset = 0;
  var isGap = false;
  var gaps = [];

  /* Find all gaps. */
  visit(ast, one);

  /* Get the end of the document.
   * This detects if the last node was the last node.
   * If not, there’s an extra gap between the last node
   * and the end of the document. */
  if (
    lastNode &&
    lastNode.position &&
    lastNode.position.end &&
    offset === lastNode.position.end.offset &&
    trim(file.toString().slice(offset)) !== ''
  ) {
    update();

    update(
      ast &&
      ast.position &&
      ast.position.end &&
      ast.position.end.offset - 1
    );
  }

  return gaps;

  function one(node) {
    var pos = node.position;

    update(pos && pos.start && pos.start.offset);

    if (!node.children) {
      update(pos && pos.end && pos.end.offset);
    }
  }

  /* Detect a new position. */
  function update(latest) {
    if (latest == null) {
      isGap = true;
      return;
    }

    if (offset >= latest) {
      return;
    }

    if (isGap) {
      gaps.push({start: offset, end: latest});
      isGap = false;
    }

    offset = latest;
  }
}
