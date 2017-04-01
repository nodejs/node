/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenizer
 * @fileoverview Markdown tokenizer.
 */

'use strict';

/* Expose. */
module.exports = factory;

/* Define nodes of a type which can be merged. */
var MERGEABLE_NODES = {};

/**
 * Check whether a node is mergeable with adjacent nodes.
 *
 * @param {Object} node - Node to check.
 * @return {boolean} - Whether `node` is mergable.
 */
function mergeable(node) {
  var start;
  var end;

  if (node.type !== 'text' || !node.position) {
    return true;
  }

  start = node.position.start;
  end = node.position.end;

  /* Only merge nodes which occupy the same size as their
   * `value`. */
  return start.line !== end.line ||
      end.column - start.column === node.value.length;
}

/**
 * Merge two text nodes: `node` into `prev`.
 *
 * @param {Object} prev - Preceding sibling.
 * @param {Object} node - Following sibling.
 * @return {Object} - `prev`.
 */
MERGEABLE_NODES.text = function (prev, node) {
  prev.value += node.value;

  return prev;
};

/**
 * Merge two blockquotes: `node` into `prev`, unless in
 * CommonMark mode.
 *
 * @param {Object} prev - Preceding sibling.
 * @param {Object} node - Following sibling.
 * @return {Object} - `prev`, or `node` in CommonMark mode.
 */
MERGEABLE_NODES.blockquote = function (prev, node) {
  if (this.options.commonmark) {
    return node;
  }

  prev.children = prev.children.concat(node.children);

  return prev;
};

/**
 * Construct a tokenizer.  This creates both
 * `tokenizeInline` and `tokenizeBlock`.
 *
 * @example
 *   Parser.prototype.tokenizeInline = tokenizeFactory('inline');
 *
 * @param {string} type - Name of parser, used to find
 *   its expressions (`%sMethods`) and tokenizers
 *   (`%Tokenizers`).
 * @return {Function} - Tokenizer.
 */
function factory(type) {
  return tokenize;

  /**
   * Tokenizer for a bound `type`
   *
   * @example
   *   parser = new Parser();
   *   parser.tokenizeInline('_foo_');
   *
   * @param {string} value - Content.
   * @param {Object} location - Offset at which `value`
   *   starts.
   * @return {Array.<Object>} - Nodes.
   */
  function tokenize(value, location) {
    var self = this;
    var offset = self.offset;
    var tokens = [];
    var methods = self[type + 'Methods'];
    var tokenizers = self[type + 'Tokenizers'];
    var line = location.line;
    var column = location.column;
    var index;
    var length;
    var method;
    var name;
    var matched;
    var valueLength;

    /* Trim white space only lines. */
    if (!value) {
      return tokens;
    }

    /* Expose on `eat`. */
    eat.now = now;
    eat.file = self.file;

    /* Sync initial offset. */
    updatePosition('');

    /* Iterate over `value`, and iterate over all
     * tokenizers.  When one eats something, re-iterate
     * with the remaining value.  If no tokenizer eats,
     * something failed (should not happen) and an
     * exception is thrown. */
    while (value) {
      index = -1;
      length = methods.length;
      matched = false;

      while (++index < length) {
        name = methods[index];
        method = tokenizers[name];

        if (
          method &&
          (!method.onlyAtStart || self.atStart) &&
          (!method.notInList || !self.inList) &&
          (!method.notInBlock || !self.inBlock) &&
          (!method.notInLink || !self.inLink)
        ) {
          valueLength = value.length;

          method.apply(self, [eat, value]);

          matched = valueLength !== value.length;

          if (matched) {
            break;
          }
        }
      }

      /* istanbul ignore if */
      if (!matched) {
        self.file.fail(new Error('Infinite loop'), eat.now());
      }
    }

    self.eof = now();

    return tokens;

    /**
     * Update line, column, and offset based on
     * `value`.
     *
     * @example
     *   updatePosition('foo');
     *
     * @param {string} subvalue - Subvalue to eat.
     */
    function updatePosition(subvalue) {
      var lastIndex = -1;
      var index = subvalue.indexOf('\n');

      while (index !== -1) {
        line++;
        lastIndex = index;
        index = subvalue.indexOf('\n', index + 1);
      }

      if (lastIndex === -1) {
        column += subvalue.length;
      } else {
        column = subvalue.length - lastIndex;
      }

      if (line in offset) {
        if (lastIndex !== -1) {
          column += offset[line];
        } else if (column <= offset[line]) {
          column = offset[line] + 1;
        }
      }
    }

    /**
     * Get offset.  Called before the first character is
     * eaten to retrieve the range's offsets.
     *
     * @return {Function} - `done`, to be called when
     *   the last character is eaten.
     */
    function getOffset() {
      var indentation = [];
      var pos = line + 1;

      /**
       * Done.  Called when the last character is
       * eaten to retrieve the range’s offsets.
       *
       * @return {Array.<number>} - Offset.
       */
      return function () {
        var last = line + 1;

        while (pos < last) {
          indentation.push((offset[pos] || 0) + 1);

          pos++;
        }

        return indentation;
      };
    }

    /**
     * Get the current position.
     *
     * @example
     *   position = now(); // {line: 1, column: 1, offset: 0}
     *
     * @return {Object} - Current Position.
     */
    function now() {
      var pos = {line: line, column: column};

      pos.offset = self.toOffset(pos);

      return pos;
    }

    /**
     * Store position information for a node.
     *
     * @example
     *   start = now();
     *   updatePosition('foo');
     *   location = new Position(start);
     *   // {
     *   //   start: {line: 1, column: 1, offset: 0},
     *   //   end: {line: 1, column: 3, offset: 2}
     *   // }
     *
     * @param {Object} start - Starting position.
     */
    function Position(start) {
      this.start = start;
      this.end = now();
    }

    /**
     * Throw when a value is incorrectly eaten.
     * This shouldn’t happen but will throw on new,
     * incorrect rules.
     *
     * @example
     *   // When the current value is set to `foo bar`.
     *   validateEat('foo');
     *   eat('foo');
     *
     *   validateEat('bar');
     *   // throws, because the space is not eaten.
     *
     * @param {string} subvalue - Value to be eaten.
     * @throws {Error} - When `subvalue` cannot be eaten.
     */
    function validateEat(subvalue) {
      /* istanbul ignore if */
      if (value.substring(0, subvalue.length) !== subvalue) {
        /* Capture stack-trace. */
        self.file.fail(
          new Error(
            'Incorrectly eaten value: please report this ' +
            'warning on http://git.io/vg5Ft'
          ),
          now()
        );
      }
    }

    /**
     * Mark position and patch `node.position`.
     *
     * @example
     *   var update = position();
     *   updatePosition('foo');
     *   update({});
     *   // {
     *   //   position: {
     *   //     start: {line: 1, column: 1, offset: 0},
     *   //     end: {line: 1, column: 3, offset: 2}
     *   //   }
     *   // }
     *
     * @returns {Function} - Updater.
     */
    function position() {
      var before = now();

      return update;

      /**
       * Add the position to a node.
       *
       * @example
       *   update({type: 'text', value: 'foo'});
       *
       * @param {Node} node - Node to attach position
       *   on.
       * @param {Array} [indent] - Indentation for
       *   `node`.
       * @return {Node} - `node`.
       */
      function update(node, indent) {
        var prev = node.position;
        var start = prev ? prev.start : before;
        var combined = [];
        var n = prev && prev.end.line;
        var l = before.line;

        node.position = new Position(start);

        /* If there was already a `position`, this
         * node was merged.  Fixing `start` wasn’t
         * hard, but the indent is different.
         * Especially because some information, the
         * indent between `n` and `l` wasn’t
         * tracked.  Luckily, that space is
         * (should be?) empty, so we can safely
         * check for it now. */
        if (prev && indent && prev.indent) {
          combined = prev.indent;

          if (n < l) {
            while (++n < l) {
              combined.push((offset[n] || 0) + 1);
            }

            combined.push(before.column);
          }

          indent = combined.concat(indent);
        }

        node.position.indent = indent || [];

        return node;
      }
    }

    /**
     * Add `node` to `parent`s children or to `tokens`.
     * Performs merges where possible.
     *
     * @example
     *   add({});
     *
     *   add({}, {children: []});
     *
     * @param {Object} node - Node to add.
     * @param {Object} [parent] - Parent to insert into.
     * @return {Object} - Added or merged into node.
     */
    function add(node, parent) {
      var children = parent ? parent.children : tokens;
      var prev = children[children.length - 1];

      if (
        prev &&
        node.type === prev.type &&
        node.type in MERGEABLE_NODES &&
        mergeable(prev) &&
        mergeable(node)
      ) {
        node = MERGEABLE_NODES[node.type].call(self, prev, node);
      }

      if (node !== prev) {
        children.push(node);
      }

      if (self.atStart && tokens.length) {
        self.exitStart();
      }

      return node;
    }

    /**
     * Remove `subvalue` from `value`.
     * `subvalue` must be at the start of `value`.
     *
     * @example
     *   eat('foo')({type: 'text', value: 'foo'});
     *
     * @param {string} subvalue - Removed from `value`,
     *   and passed to `updatePosition`.
     * @return {Function} - Wrapper around `add`, which
     *   also adds `position` to node.
     */
    function eat(subvalue) {
      var indent = getOffset();
      var pos = position();
      var current = now();

      validateEat(subvalue);

      apply.reset = reset;
      apply.test = reset.test = test;

      value = value.substring(subvalue.length);

      updatePosition(subvalue);

      indent = indent();

      return apply;

      /**
       * Add the given arguments, add `position` to
       * the returned node, and return the node.
       *
       * @param {Object} node - Node to add.
       * @param {Object} [parent] - Node to insert into.
       * @return {Node} - Added node.
       */
      function apply(node, parent) {
        return pos(add(pos(node), parent), indent);
      }

      /**
       * Functions just like apply, but resets the
       * content:  the line and column are reversed,
       * and the eaten value is re-added.
       *
       * This is useful for nodes with a single
       * type of content, such as lists and tables.
       *
       * See `apply` above for what parameters are
       * expected.
       *
       * @return {Node} - Added node.
       */
      function reset() {
        var node = apply.apply(null, arguments);

        line = current.line;
        column = current.column;
        value = subvalue + value;

        return node;
      }

      /**
       * Test the position, after eating, and reverse
       * to a not-eaten state.
       *
       * @return {Position} - Position after eating `subvalue`.
       */
      function test() {
        var result = pos({});

        line = current.line;
        column = current.column;
        value = subvalue + value;

        return result.position;
      }
    }
  }
}
