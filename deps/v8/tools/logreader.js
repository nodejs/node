// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @fileoverview Log Reader is used to process log file produced by V8.
 */

// Initlialize namespaces
var devtools = devtools || {};
devtools.profiler = devtools.profiler || {};


/**
 * Base class for processing log files.
 *
 * @param {Array.<Object>} dispatchTable A table used for parsing and processing
 *     log records.
 * @constructor
 */
devtools.profiler.LogReader = function(dispatchTable) {
  /**
   * @type {Array.<Object>}
   */
  this.dispatchTable_ = dispatchTable;
  this.dispatchTable_['alias'] =
      { parsers: [null, null], processor: this.processAlias_ };
  this.dispatchTable_['repeat'] =
      { parsers: [parseInt, 'var-args'], processor: this.processRepeat_,
        backrefs: true };

  /**
   * A key-value map for aliases. Translates short name -> full name.
   * @type {Object}
   */
  this.aliases_ = {};

  /**
   * A key-value map for previous address values.
   * @type {Object}
   */
  this.prevAddresses_ = {};

  /**
   * A key-value map for events than can be backreference-compressed.
   * @type {Object}
   */
  this.backRefsCommands_ = {};
  this.initBackRefsCommands_();

  /**
   * Back references for decompression.
   * @type {Array.<string>}
   */
  this.backRefs_ = [];

  /**
   * Current line.
   * @type {number}
   */
  this.lineNum_ = 0;

  /**
   * CSV lines parser.
   * @type {devtools.profiler.CsvParser}
   */
  this.csvParser_ = new devtools.profiler.CsvParser();
};


/**
 * Creates a parser for an address entry.
 *
 * @param {string} addressTag Address tag to perform offset decoding.
 * @return {function(string):number} Address parser.
 */
devtools.profiler.LogReader.prototype.createAddressParser = function(
    addressTag) {
  var self = this;
  return (function (str) {
    var value = parseInt(str, 16);
    var firstChar = str.charAt(0);
    if (firstChar == '+' || firstChar == '-') {
      var addr = self.prevAddresses_[addressTag];
      addr += value;
      self.prevAddresses_[addressTag] = addr;
      return addr;
    } else if (firstChar != '0' || str.charAt(1) != 'x') {
      self.prevAddresses_[addressTag] = value;
    }
    return value;
  });
};


/**
 * Expands an alias symbol, if applicable.
 *
 * @param {string} symbol Symbol to expand.
 * @return {string} Expanded symbol, or the input symbol itself.
 */
devtools.profiler.LogReader.prototype.expandAlias = function(symbol) {
  return symbol in this.aliases_ ? this.aliases_[symbol] : symbol;
};


/**
 * Used for printing error messages.
 *
 * @param {string} str Error message.
 */
devtools.profiler.LogReader.prototype.printError = function(str) {
  // Do nothing.
};


/**
 * Processes a portion of V8 profiler event log.
 *
 * @param {string} chunk A portion of log.
 */
devtools.profiler.LogReader.prototype.processLogChunk = function(chunk) {
  this.processLog_(chunk.split('\n'));
};


/**
 * Processes a line of V8 profiler event log.
 *
 * @param {string} line A line of log.
 */
devtools.profiler.LogReader.prototype.processLogLine = function(line) {
  this.processLog_([line]);
};


/**
 * Processes stack record.
 *
 * @param {number} pc Program counter.
 * @param {number} func JS Function.
 * @param {Array.<string>} stack String representation of a stack.
 * @return {Array.<number>} Processed stack.
 */
devtools.profiler.LogReader.prototype.processStack = function(pc, func, stack) {
  var fullStack = func ? [pc, func] : [pc];
  var prevFrame = pc;
  for (var i = 0, n = stack.length; i < n; ++i) {
    var frame = stack[i];
    var firstChar = frame.charAt(0);
    if (firstChar == '+' || firstChar == '-') {
      // An offset from the previous frame.
      prevFrame += parseInt(frame, 16);
      fullStack.push(prevFrame);
    // Filter out possible 'overflow' string.
    } else if (firstChar != 'o') {
      fullStack.push(parseInt(frame, 16));
    }
  }
  return fullStack;
};


/**
 * Returns whether a particular dispatch must be skipped.
 *
 * @param {!Object} dispatch Dispatch record.
 * @return {boolean} True if dispatch must be skipped.
 */
devtools.profiler.LogReader.prototype.skipDispatch = function(dispatch) {
  return false;
};


/**
 * Does a dispatch of a log record.
 *
 * @param {Array.<string>} fields Log record.
 * @private
 */
devtools.profiler.LogReader.prototype.dispatchLogRow_ = function(fields) {
  // Obtain the dispatch.
  var command = fields[0];
  if (!(command in this.dispatchTable_)) {
    throw new Error('unknown command: ' + command);
  }
  var dispatch = this.dispatchTable_[command];

  if (dispatch === null || this.skipDispatch(dispatch)) {
    return;
  }

  // Parse fields.
  var parsedFields = [];
  for (var i = 0; i < dispatch.parsers.length; ++i) {
    var parser = dispatch.parsers[i];
    if (parser === null) {
      parsedFields.push(fields[1 + i]);
    } else if (typeof parser == 'function') {
      parsedFields.push(parser(fields[1 + i]));
    } else {
      // var-args
      parsedFields.push(fields.slice(1 + i));
      break;
    }
  }

  // Run the processor.
  dispatch.processor.apply(this, parsedFields);
};


/**
 * Decompresses a line if it was backreference-compressed.
 *
 * @param {string} line Possibly compressed line.
 * @return {string} Decompressed line.
 * @private
 */
devtools.profiler.LogReader.prototype.expandBackRef_ = function(line) {
  var backRefPos;
  // Filter out case when a regexp is created containing '#'.
  if (line.charAt(line.length - 1) != '"'
      && (backRefPos = line.lastIndexOf('#')) != -1) {
    var backRef = line.substr(backRefPos + 1);
    var backRefIdx = parseInt(backRef, 10) - 1;
    var colonPos = backRef.indexOf(':');
    var backRefStart =
        colonPos != -1 ? parseInt(backRef.substr(colonPos + 1), 10) : 0;
    line = line.substr(0, backRefPos) +
        this.backRefs_[backRefIdx].substr(backRefStart);
  }
  this.backRefs_.unshift(line);
  if (this.backRefs_.length > 10) {
    this.backRefs_.length = 10;
  }
  return line;
};


/**
 * Initializes the map of backward reference compressible commands.
 * @private
 */
devtools.profiler.LogReader.prototype.initBackRefsCommands_ = function() {
  for (var event in this.dispatchTable_) {
    var dispatch = this.dispatchTable_[event];
    if (dispatch && dispatch.backrefs) {
      this.backRefsCommands_[event] = true;
    }
  }
};


/**
 * Processes alias log record. Adds an alias to a corresponding map.
 *
 * @param {string} symbol Short name.
 * @param {string} expansion Long name.
 * @private
 */
devtools.profiler.LogReader.prototype.processAlias_ = function(
    symbol, expansion) {
  if (expansion in this.dispatchTable_) {
    this.dispatchTable_[symbol] = this.dispatchTable_[expansion];
    if (expansion in this.backRefsCommands_) {
      this.backRefsCommands_[symbol] = true;
    }
  } else {
    this.aliases_[symbol] = expansion;
  }
};


/**
 * Processes log lines.
 *
 * @param {Array.<string>} lines Log lines.
 * @private
 */
devtools.profiler.LogReader.prototype.processLog_ = function(lines) {
  for (var i = 0, n = lines.length; i < n; ++i, ++this.lineNum_) {
    var line = lines[i];
    if (!line) {
      continue;
    }
    try {
      if (line.charAt(0) == '#' ||
          line.substr(0, line.indexOf(',')) in this.backRefsCommands_) {
        line = this.expandBackRef_(line);
      }
      var fields = this.csvParser_.parseLine(line);
      this.dispatchLogRow_(fields);
    } catch (e) {
      this.printError('line ' + (this.lineNum_ + 1) + ': ' + (e.message || e));
    }
  }
};


/**
 * Processes repeat log record. Expands it according to calls count and
 * invokes processing.
 *
 * @param {number} count Count.
 * @param {Array.<string>} cmd Parsed command.
 * @private
 */
devtools.profiler.LogReader.prototype.processRepeat_ = function(count, cmd) {
  // Replace the repeat-prefixed command from backrefs list with a non-prefixed.
  this.backRefs_[0] = cmd.join(',');
  for (var i = 0; i < count; ++i) {
    this.dispatchLogRow_(cmd);
  }
};
