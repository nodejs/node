// Copyright 2011 the V8 project authors. All rights reserved.
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

// This is a supplementary file for test-log/EquivalenceOfLoggingAndTraversal.

function parseState(s) {
  switch (s) {
  case "": return Profile.CodeState.COMPILED;
  case "~": return Profile.CodeState.OPTIMIZABLE;
  case "*": return Profile.CodeState.OPTIMIZED;
  }
  throw new Error("unknown code state: " + s);
}

function LogProcessor() {
  LogReader.call(this, {
      'code-creation': {
          parsers: [null, parseInt, parseInt, null, 'var-args'],
          processor: this.processCodeCreation },
      'code-move': { parsers: [parseInt, parseInt],
          processor: this.processCodeMove },
      'code-delete': null,
      'sfi-move': { parsers: [parseInt, parseInt],
          processor: this.processFunctionMove },
      'shared-library': null,
      'profiler': null,
      'tick': null });
  this.profile = new Profile();

}
LogProcessor.prototype.__proto__ = LogReader.prototype;

LogProcessor.prototype.processCodeCreation = function(
    type, start, size, name, maybe_func) {
  if (type != "LazyCompile" && type != "Script" && type != "Function") return;
  // Discard types to avoid discrepancies in "LazyCompile" vs. "Function".
  type = "";
  if (maybe_func.length) {
    var funcAddr = parseInt(maybe_func[0]);
    var state = parseState(maybe_func[1]);
    this.profile.addFuncCode(type, name, start, size, funcAddr, state);
  } else {
    this.profile.addCode(type, name, start, size);
  }
};

LogProcessor.prototype.processCodeMove = function(from, to) {
  this.profile.moveCode(from, to);
};

LogProcessor.prototype.processFunctionMove = function(from, to) {
  this.profile.moveFunc(from, to);
};

function RunTest() {
  // _log must be provided externally.
  var log_lines = _log.split("\n");
  var line, pos = 0, log_lines_length = log_lines.length;
  if (log_lines_length < 2)
    return "log_lines_length < 2";
  var logging_processor = new LogProcessor();
  for ( ; pos < log_lines_length; ++pos) {
    line = log_lines[pos];
    if (line === "test-logging-done,\"\"") {
      ++pos;
      break;
    }
    logging_processor.processLogLine(line);
  }
  logging_processor.profile.cleanUpFuncEntries();
  var logging_entries =
    logging_processor.profile.codeMap_.getAllDynamicEntriesWithAddresses();
  if (logging_entries.length === 0)
    return "logging_entries.length === 0";
  var traversal_processor = new LogProcessor();
  for ( ; pos < log_lines_length; ++pos) {
    line = log_lines[pos];
    if (line === "test-traversal-done,\"\"") break;
    traversal_processor.processLogLine(line);
  }
  var traversal_entries =
    traversal_processor.profile.codeMap_.getAllDynamicEntriesWithAddresses();
  if (traversal_entries.length === 0)
    return "traversal_entries.length === 0";

  function addressComparator(entryA, entryB) {
    return entryA[0] < entryB[0] ? -1 : (entryA[0] > entryB[0] ? 1 : 0);
  }

  logging_entries.sort(addressComparator);
  traversal_entries.sort(addressComparator);

  function entityNamesEqual(entityA, entityB) {
    if ("getRawName" in entityB &&
        entityNamesEqual.builtins.indexOf(entityB.getRawName()) !== -1) {
      return true;
    }
    if (entityNamesEqual.builtins.indexOf(entityB.getName()) !== -1) return true;
    return entityA.getName() === entityB.getName();
  }
  entityNamesEqual.builtins =
    ["Boolean", "Function", "Number", "Object",
     "Script", "String", "RegExp", "Date", "Error"];

  function entitiesEqual(entityA, entityB) {
    if ((entityA === null && entityB !== null) ||
      (entityA !== null && entityB === null)) return true;
    return entityA.size === entityB.size && entityNamesEqual(entityA, entityB);
  }

  var l_pos = 0, t_pos = 0;
  var l_len = logging_entries.length, t_len = traversal_entries.length;
  var comparison = [];
  var equal = true;
  // Do a merge-like comparison of entries. At the same address we expect to
  // find the same entries. We skip builtins during log parsing, but compiled
  // functions traversal may erroneously recognize them as functions, so we are
  // expecting more functions in traversal vs. logging.
  // Since we don't track code deletions, logging can also report more entries
  // than traversal.
  while (l_pos < l_len && t_pos < t_len) {
    var entryA = logging_entries[l_pos];
    var entryB = traversal_entries[t_pos];
    var cmp = addressComparator(entryA, entryB);
    var entityA = entryA[1], entityB = entryB[1];
    var address = entryA[0];
    if (cmp < 0) {
      ++l_pos;
      entityB = null;
    } else if (cmp > 0) {
      ++t_pos;
      entityA = null;
      address = entryB[0];
    } else {
      ++l_pos;
      ++t_pos;
    }
    var entities_equal = entitiesEqual(entityA, entityB);
    if (!entities_equal) equal = false;
    comparison.push([entities_equal, address, entityA, entityB]);
  }
  return [equal, comparison];
}

var result = RunTest();
if (typeof result !== "string") {
  var out = [];
  if (!result[0]) {
    var comparison = result[1];
    for (var i = 0, l = comparison.length; i < l; ++i) {
      var c = comparison[i];
      out.push((c[0] ? "  " : "* ") +
               c[1].toString(16) + " " +
               (c[2] ? c[2] : "---") + " " +
               (c[3] ? c[3] : "---"));
    }
  }
  result[0] ? true : out.join("\n");
} else {
  result;
}
