// Copyright 2012 the V8 project authors. All rights reserved.
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

function inherits(childCtor, parentCtor) {
  childCtor.prototype.__proto__ = parentCtor.prototype;
};


function V8Profile(separateIc, separateBytecodes, separateBuiltins,
    separateStubs) {
  Profile.call(this);
  var regexps = [];
  if (!separateIc) regexps.push(V8Profile.IC_RE);
  if (!separateBytecodes) regexps.push(V8Profile.BYTECODES_RE);
  if (!separateBuiltins) regexps.push(V8Profile.BUILTINS_RE);
  if (!separateStubs) regexps.push(V8Profile.STUBS_RE);
  if (regexps.length > 0) {
    this.skipThisFunction = function(name) {
      for (var i=0; i<regexps.length; i++) {
        if (regexps[i].test(name)) return true;
      }
      return false;
    };
  }
};
inherits(V8Profile, Profile);


V8Profile.IC_RE =
    /^(LoadGlobalIC: )|(Handler: )|(?:CallIC|LoadIC|StoreIC)|(?:Builtin: (?:Keyed)?(?:Load|Store)IC_)/;
V8Profile.BYTECODES_RE = /^(BytecodeHandler: )/
V8Profile.BUILTINS_RE = /^(Builtin: )/
V8Profile.STUBS_RE = /^(Stub: )/


/**
 * A thin wrapper around shell's 'read' function showing a file name on error.
 */
function readFile(fileName) {
  try {
    return read(fileName);
  } catch (e) {
    printErr(fileName + ': ' + (e.message || e));
    throw e;
  }
}


/**
 * Parser for dynamic code optimization state.
 */
function parseState(s) {
  switch (s) {
  case "": return Profile.CodeState.COMPILED;
  case "~": return Profile.CodeState.OPTIMIZABLE;
  case "*": return Profile.CodeState.OPTIMIZED;
  }
  throw new Error("unknown code state: " + s);
}


function TickProcessor(
    cppEntriesProvider,
    separateIc,
    separateBytecodes,
    separateBuiltins,
    separateStubs,
    callGraphSize,
    ignoreUnknown,
    stateFilter,
    distortion,
    range,
    sourceMap,
    timedRange,
    pairwiseTimedRange,
    onlySummary,
    runtimeTimerFilter,
    preprocessJson) {
  this.preprocessJson = preprocessJson;
  LogReader.call(this, {
      'shared-library': { parsers: [parseString, parseInt, parseInt, parseInt],
          processor: this.processSharedLibrary },
      'code-creation': {
          parsers: [parseString, parseInt, parseInt, parseInt, parseInt,
                    parseString, parseVarArgs],
          processor: this.processCodeCreation },
      'code-deopt': {
          parsers: [parseInt, parseInt, parseInt, parseInt, parseInt,
                    parseString, parseString, parseString],
          processor: this.processCodeDeopt },
      'code-move': { parsers: [parseInt, parseInt, ],
          processor: this.processCodeMove },
      'code-delete': { parsers: [parseInt],
          processor: this.processCodeDelete },
      'code-source-info': {
          parsers: [parseInt, parseInt, parseInt, parseInt, parseString,
                    parseString, parseString],
          processor: this.processCodeSourceInfo },
      'script-source': {
          parsers: [parseInt, parseString, parseString],
          processor: this.processScriptSource },
      'sfi-move': { parsers: [parseInt, parseInt],
          processor: this.processFunctionMove },
      'active-runtime-timer': {
        parsers: [parseString],
        processor: this.processRuntimeTimerEvent },
      'tick': {
          parsers: [parseInt, parseInt, parseInt,
                    parseInt, parseInt, parseVarArgs],
          processor: this.processTick },
      'heap-sample-begin': { parsers: [parseString, parseString, parseInt],
          processor: this.processHeapSampleBegin },
      'heap-sample-end': { parsers: [parseString, parseString],
          processor: this.processHeapSampleEnd },
      'timer-event-start' : { parsers: [parseString, parseString, parseString],
                              processor: this.advanceDistortion },
      'timer-event-end' : { parsers: [parseString, parseString, parseString],
                            processor: this.advanceDistortion },
      // Ignored events.
      'profiler': null,
      'function-creation': null,
      'function-move': null,
      'function-delete': null,
      'heap-sample-item': null,
      'current-time': null,  // Handled specially, not parsed.
      // Obsolete row types.
      'code-allocate': null,
      'begin-code-region': null,
      'end-code-region': null },
      timedRange,
      pairwiseTimedRange);

  this.cppEntriesProvider_ = cppEntriesProvider;
  this.callGraphSize_ = callGraphSize;
  this.ignoreUnknown_ = ignoreUnknown;
  this.stateFilter_ = stateFilter;
  this.runtimeTimerFilter_ = runtimeTimerFilter;
  this.sourceMap = sourceMap;
  var ticks = this.ticks_ =
    { total: 0, unaccounted: 0, excluded: 0, gc: 0 };

  distortion = parseInt(distortion);
  // Convert picoseconds to nanoseconds.
  this.distortion_per_entry = isNaN(distortion) ? 0 : (distortion / 1000);
  this.distortion = 0;
  var rangelimits = range ? range.split(",") : [];
  var range_start = parseInt(rangelimits[0]);
  var range_end = parseInt(rangelimits[1]);
  // Convert milliseconds to nanoseconds.
  this.range_start = isNaN(range_start) ? -Infinity : (range_start * 1000);
  this.range_end = isNaN(range_end) ? Infinity : (range_end * 1000)

  V8Profile.prototype.handleUnknownCode = function(
      operation, addr, opt_stackPos) {
    var op = Profile.Operation;
    switch (operation) {
      case op.MOVE:
        printErr('Code move event for unknown code: 0x' + addr.toString(16));
        break;
      case op.DELETE:
        printErr('Code delete event for unknown code: 0x' + addr.toString(16));
        break;
      case op.TICK:
        // Only unknown PCs (the first frame) are reported as unaccounted,
        // otherwise tick balance will be corrupted (this behavior is compatible
        // with the original tickprocessor.py script.)
        if (opt_stackPos == 0) {
          ticks.unaccounted++;
        }
        break;
    }
  };

  if (preprocessJson) {
    this.profile_ = new JsonProfile();
  } else {
    this.profile_ = new V8Profile(separateIc, separateBytecodes,
        separateBuiltins, separateStubs);
  }
  this.codeTypes_ = {};
  // Count each tick as a time unit.
  this.viewBuilder_ = new ViewBuilder(1);
  this.lastLogFileName_ = null;

  this.generation_ = 1;
  this.currentProducerProfile_ = null;
  this.onlySummary_ = onlySummary;
};
inherits(TickProcessor, LogReader);


TickProcessor.VmStates = {
  JS: 0,
  GC: 1,
  PARSER: 2,
  BYTECODE_COMPILER: 3,
  COMPILER: 4,
  OTHER: 5,
  EXTERNAL: 6,
  IDLE: 7,
};


TickProcessor.CodeTypes = {
  CPP: 0,
  SHARED_LIB: 1
};
// Otherwise, this is JS-related code. We are not adding it to
// codeTypes_ map because there can be zillions of them.


TickProcessor.CALL_PROFILE_CUTOFF_PCT = 1.0;

TickProcessor.CALL_GRAPH_SIZE = 5;

/**
 * @override
 */
TickProcessor.prototype.printError = function(str) {
  printErr(str);
};


TickProcessor.prototype.setCodeType = function(name, type) {
  this.codeTypes_[name] = TickProcessor.CodeTypes[type];
};


TickProcessor.prototype.isSharedLibrary = function(name) {
  return this.codeTypes_[name] == TickProcessor.CodeTypes.SHARED_LIB;
};


TickProcessor.prototype.isCppCode = function(name) {
  return this.codeTypes_[name] == TickProcessor.CodeTypes.CPP;
};


TickProcessor.prototype.isJsCode = function(name) {
  return name !== "UNKNOWN" && !(name in this.codeTypes_);
};


TickProcessor.prototype.processLogFile = function(fileName) {
  this.lastLogFileName_ = fileName;
  var line;
  while (line = readline()) {
    this.processLogLine(line);
  }
};


TickProcessor.prototype.processLogFileInTest = function(fileName) {
   // Hack file name to avoid dealing with platform specifics.
  this.lastLogFileName_ = 'v8.log';
  var contents = readFile(fileName);
  this.processLogChunk(contents);
};


TickProcessor.prototype.processSharedLibrary = function(
    name, startAddr, endAddr, aslrSlide) {
  var entry = this.profile_.addLibrary(name, startAddr, endAddr, aslrSlide);
  this.setCodeType(entry.getName(), 'SHARED_LIB');

  var self = this;
  var libFuncs = this.cppEntriesProvider_.parseVmSymbols(
      name, startAddr, endAddr, aslrSlide, function(fName, fStart, fEnd) {
    self.profile_.addStaticCode(fName, fStart, fEnd);
    self.setCodeType(fName, 'CPP');
  });
};


TickProcessor.prototype.processCodeCreation = function(
    type, kind, timestamp, start, size, name, maybe_func) {
  if (maybe_func.length) {
    var funcAddr = parseInt(maybe_func[0]);
    var state = parseState(maybe_func[1]);
    this.profile_.addFuncCode(type, name, timestamp, start, size, funcAddr, state);
  } else {
    this.profile_.addCode(type, name, timestamp, start, size);
  }
};


TickProcessor.prototype.processCodeDeopt = function(
    timestamp, size, code, inliningId, scriptOffset, bailoutType,
    sourcePositionText, deoptReasonText) {
  this.profile_.deoptCode(timestamp, code, inliningId, scriptOffset,
      bailoutType, sourcePositionText, deoptReasonText);
};


TickProcessor.prototype.processCodeMove = function(from, to) {
  this.profile_.moveCode(from, to);
};

TickProcessor.prototype.processCodeDelete = function(start) {
  this.profile_.deleteCode(start);
};

TickProcessor.prototype.processCodeSourceInfo = function(
    start, script, startPos, endPos, sourcePositions, inliningPositions,
    inlinedFunctions) {
  this.profile_.addSourcePositions(start, script, startPos,
    endPos, sourcePositions, inliningPositions, inlinedFunctions);
};

TickProcessor.prototype.processScriptSource = function(script, url, source) {
  this.profile_.addScriptSource(script, url, source);
};

TickProcessor.prototype.processFunctionMove = function(from, to) {
  this.profile_.moveFunc(from, to);
};


TickProcessor.prototype.includeTick = function(vmState) {
  if (this.stateFilter_ !== null) {
    return this.stateFilter_ == vmState;
  } else if (this.runtimeTimerFilter_ !== null) {
    return this.currentRuntimeTimer == this.runtimeTimerFilter_;
  }
  return true;
};

TickProcessor.prototype.processRuntimeTimerEvent = function(name) {
  this.currentRuntimeTimer = name;
}

TickProcessor.prototype.processTick = function(pc,
                                               ns_since_start,
                                               is_external_callback,
                                               tos_or_external_callback,
                                               vmState,
                                               stack) {
  this.distortion += this.distortion_per_entry;
  ns_since_start -= this.distortion;
  if (ns_since_start < this.range_start || ns_since_start > this.range_end) {
    return;
  }
  this.ticks_.total++;
  if (vmState == TickProcessor.VmStates.GC) this.ticks_.gc++;
  if (!this.includeTick(vmState)) {
    this.ticks_.excluded++;
    return;
  }
  if (is_external_callback) {
    // Don't use PC when in external callback code, as it can point
    // inside callback's code, and we will erroneously report
    // that a callback calls itself. Instead we use tos_or_external_callback,
    // as simply resetting PC will produce unaccounted ticks.
    pc = tos_or_external_callback;
    tos_or_external_callback = 0;
  } else if (tos_or_external_callback) {
    // Find out, if top of stack was pointing inside a JS function
    // meaning that we have encountered a frameless invocation.
    var funcEntry = this.profile_.findEntry(tos_or_external_callback);
    if (!funcEntry || !funcEntry.isJSFunction || !funcEntry.isJSFunction()) {
      tos_or_external_callback = 0;
    }
  }

  this.profile_.recordTick(
      ns_since_start, vmState,
      this.processStack(pc, tos_or_external_callback, stack));
};


TickProcessor.prototype.advanceDistortion = function() {
  this.distortion += this.distortion_per_entry;
}


TickProcessor.prototype.processHeapSampleBegin = function(space, state, ticks) {
  if (space != 'Heap') return;
  this.currentProducerProfile_ = new CallTree();
};


TickProcessor.prototype.processHeapSampleEnd = function(space, state) {
  if (space != 'Heap' || !this.currentProducerProfile_) return;

  print('Generation ' + this.generation_ + ':');
  var tree = this.currentProducerProfile_;
  tree.computeTotalWeights();
  var producersView = this.viewBuilder_.buildView(tree);
  // Sort by total time, desc, then by name, desc.
  producersView.sort(function(rec1, rec2) {
      return rec2.totalTime - rec1.totalTime ||
          (rec2.internalFuncName < rec1.internalFuncName ? -1 : 1); });
  this.printHeavyProfile(producersView.head.children);

  this.currentProducerProfile_ = null;
  this.generation_++;
};


TickProcessor.prototype.printStatistics = function() {
  if (this.preprocessJson) {
    this.profile_.writeJson();
    return;
  }

  print('Statistical profiling result from ' + this.lastLogFileName_ +
        ', (' + this.ticks_.total +
        ' ticks, ' + this.ticks_.unaccounted + ' unaccounted, ' +
        this.ticks_.excluded + ' excluded).');

  if (this.ticks_.total == 0) return;

  var flatProfile = this.profile_.getFlatProfile();
  var flatView = this.viewBuilder_.buildView(flatProfile);
  // Sort by self time, desc, then by name, desc.
  flatView.sort(function(rec1, rec2) {
      return rec2.selfTime - rec1.selfTime ||
          (rec2.internalFuncName < rec1.internalFuncName ? -1 : 1); });
  var totalTicks = this.ticks_.total;
  if (this.ignoreUnknown_) {
    totalTicks -= this.ticks_.unaccounted;
  }
  var printAllTicks = !this.onlySummary_;

  // Count library ticks
  var flatViewNodes = flatView.head.children;
  var self = this;

  var libraryTicks = 0;
  if(printAllTicks) this.printHeader('Shared libraries');
  this.printEntries(flatViewNodes, totalTicks, null,
      function(name) { return self.isSharedLibrary(name); },
      function(rec) { libraryTicks += rec.selfTime; }, printAllTicks);
  var nonLibraryTicks = totalTicks - libraryTicks;

  var jsTicks = 0;
  if(printAllTicks) this.printHeader('JavaScript');
  this.printEntries(flatViewNodes, totalTicks, nonLibraryTicks,
      function(name) { return self.isJsCode(name); },
      function(rec) { jsTicks += rec.selfTime; }, printAllTicks);

  var cppTicks = 0;
  if(printAllTicks) this.printHeader('C++');
  this.printEntries(flatViewNodes, totalTicks, nonLibraryTicks,
      function(name) { return self.isCppCode(name); },
      function(rec) { cppTicks += rec.selfTime; }, printAllTicks);

  this.printHeader('Summary');
  this.printLine('JavaScript', jsTicks, totalTicks, nonLibraryTicks);
  this.printLine('C++', cppTicks, totalTicks, nonLibraryTicks);
  this.printLine('GC', this.ticks_.gc, totalTicks, nonLibraryTicks);
  this.printLine('Shared libraries', libraryTicks, totalTicks, null);
  if (!this.ignoreUnknown_ && this.ticks_.unaccounted > 0) {
    this.printLine('Unaccounted', this.ticks_.unaccounted,
                   this.ticks_.total, null);
  }

  if(printAllTicks) {
    print('\n [C++ entry points]:');
    print('   ticks    cpp   total   name');
    var c_entry_functions = this.profile_.getCEntryProfile();
    var total_c_entry = c_entry_functions[0].ticks;
    for (var i = 1; i < c_entry_functions.length; i++) {
      c = c_entry_functions[i];
      this.printLine(c.name, c.ticks, total_c_entry, totalTicks);
    }

    this.printHeavyProfHeader();
    var heavyProfile = this.profile_.getBottomUpProfile();
    var heavyView = this.viewBuilder_.buildView(heavyProfile);
    // To show the same percentages as in the flat profile.
    heavyView.head.totalTime = totalTicks;
    // Sort by total time, desc, then by name, desc.
    heavyView.sort(function(rec1, rec2) {
        return rec2.totalTime - rec1.totalTime ||
            (rec2.internalFuncName < rec1.internalFuncName ? -1 : 1); });
    this.printHeavyProfile(heavyView.head.children);
  }
};


function padLeft(s, len) {
  s = s.toString();
  if (s.length < len) {
    var padLength = len - s.length;
    if (!(padLength in padLeft)) {
      padLeft[padLength] = new Array(padLength + 1).join(' ');
    }
    s = padLeft[padLength] + s;
  }
  return s;
};


TickProcessor.prototype.printHeader = function(headerTitle) {
  print('\n [' + headerTitle + ']:');
  print('   ticks  total  nonlib   name');
};


TickProcessor.prototype.printLine = function(
    entry, ticks, totalTicks, nonLibTicks) {
  var pct = ticks * 100 / totalTicks;
  var nonLibPct = nonLibTicks != null
      ? padLeft((ticks * 100 / nonLibTicks).toFixed(1), 5) + '%  '
      : '        ';
  print('  ' + padLeft(ticks, 5) + '  ' +
        padLeft(pct.toFixed(1), 5) + '%  ' +
        nonLibPct +
        entry);
}

TickProcessor.prototype.printHeavyProfHeader = function() {
  print('\n [Bottom up (heavy) profile]:');
  print('  Note: percentage shows a share of a particular caller in the ' +
        'total\n' +
        '  amount of its parent calls.');
  print('  Callers occupying less than ' +
        TickProcessor.CALL_PROFILE_CUTOFF_PCT.toFixed(1) +
        '% are not shown.\n');
  print('   ticks parent  name');
};


TickProcessor.prototype.processProfile = function(
    profile, filterP, func) {
  for (var i = 0, n = profile.length; i < n; ++i) {
    var rec = profile[i];
    if (!filterP(rec.internalFuncName)) {
      continue;
    }
    func(rec);
  }
};

TickProcessor.prototype.getLineAndColumn = function(name) {
  var re = /:([0-9]+):([0-9]+)$/;
  var array = re.exec(name);
  if (!array) {
    return null;
  }
  return {line: array[1], column: array[2]};
}

TickProcessor.prototype.hasSourceMap = function() {
  return this.sourceMap != null;
};


TickProcessor.prototype.formatFunctionName = function(funcName) {
  if (!this.hasSourceMap()) {
    return funcName;
  }
  var lc = this.getLineAndColumn(funcName);
  if (lc == null) {
    return funcName;
  }
  // in source maps lines and columns are zero based
  var lineNumber = lc.line - 1;
  var column = lc.column - 1;
  var entry = this.sourceMap.findEntry(lineNumber, column);
  var sourceFile = entry[2];
  var sourceLine = entry[3] + 1;
  var sourceColumn = entry[4] + 1;

  return sourceFile + ':' + sourceLine + ':' + sourceColumn + ' -> ' + funcName;
};

TickProcessor.prototype.printEntries = function(
    profile, totalTicks, nonLibTicks, filterP, callback, printAllTicks) {
  var that = this;
  this.processProfile(profile, filterP, function (rec) {
    if (rec.selfTime == 0) return;
    callback(rec);
    var funcName = that.formatFunctionName(rec.internalFuncName);
    if(printAllTicks) {
      that.printLine(funcName, rec.selfTime, totalTicks, nonLibTicks);
    }
  });
};


TickProcessor.prototype.printHeavyProfile = function(profile, opt_indent) {
  var self = this;
  var indent = opt_indent || 0;
  var indentStr = padLeft('', indent);
  this.processProfile(profile, function() { return true; }, function (rec) {
    // Cut off too infrequent callers.
    if (rec.parentTotalPercent < TickProcessor.CALL_PROFILE_CUTOFF_PCT) return;
    var funcName = self.formatFunctionName(rec.internalFuncName);
    print('  ' + padLeft(rec.totalTime, 5) + '  ' +
          padLeft(rec.parentTotalPercent.toFixed(1), 5) + '%  ' +
          indentStr + funcName);
    // Limit backtrace depth.
    if (indent < 2 * self.callGraphSize_) {
      self.printHeavyProfile(rec.children, indent + 2);
    }
    // Delimit top-level functions.
    if (indent == 0) {
      print('');
    }
  });
};


function CppEntriesProvider() {
};


CppEntriesProvider.prototype.parseVmSymbols = function(
    libName, libStart, libEnd, libASLRSlide, processorFunc) {
  this.loadSymbols(libName);

  var lastUnknownSize;
  var lastAdded;

  function inRange(funcInfo, start, end) {
    return funcInfo.start >= start && funcInfo.end <= end;
  }

  function addEntry(funcInfo) {
    // Several functions can be mapped onto the same address. To avoid
    // creating zero-sized entries, skip such duplicates.
    // Also double-check that function belongs to the library address space.

    if (lastUnknownSize &&
        lastUnknownSize.start < funcInfo.start) {
      // Try to update lastUnknownSize based on new entries start position.
      lastUnknownSize.end = funcInfo.start;
      if ((!lastAdded || !inRange(lastUnknownSize, lastAdded.start,
                                  lastAdded.end)) &&
          inRange(lastUnknownSize, libStart, libEnd)) {
        processorFunc(lastUnknownSize.name, lastUnknownSize.start,
                      lastUnknownSize.end);
        lastAdded = lastUnknownSize;
      }
    }
    lastUnknownSize = undefined;

    if (funcInfo.end) {
      // Skip duplicates that have the same start address as the last added.
      if ((!lastAdded || lastAdded.start != funcInfo.start) &&
          inRange(funcInfo, libStart, libEnd)) {
        processorFunc(funcInfo.name, funcInfo.start, funcInfo.end);
        lastAdded = funcInfo;
      }
    } else {
      // If a funcInfo doesn't have an end, try to match it up with then next
      // entry.
      lastUnknownSize = funcInfo;
    }
  }

  while (true) {
    var funcInfo = this.parseNextLine();
    if (funcInfo === null) {
      continue;
    } else if (funcInfo === false) {
      break;
    }
    if (funcInfo.start < libStart - libASLRSlide &&
        funcInfo.start < libEnd - libStart) {
      funcInfo.start += libStart;
    } else {
      funcInfo.start += libASLRSlide;
    }
    if (funcInfo.size) {
      funcInfo.end = funcInfo.start + funcInfo.size;
    }
    addEntry(funcInfo);
  }
  addEntry({name: '', start: libEnd});
};


CppEntriesProvider.prototype.loadSymbols = function(libName) {
};


CppEntriesProvider.prototype.parseNextLine = function() {
  return false;
};


function UnixCppEntriesProvider(nmExec, objdumpExec, targetRootFS, apkEmbeddedLibrary) {
  this.symbols = [];
  // File offset of a symbol minus the virtual address of a symbol found in
  // the symbol table.
  this.fileOffsetMinusVma = 0;
  this.parsePos = 0;
  this.nmExec = nmExec;
  this.objdumpExec = objdumpExec;
  this.targetRootFS = targetRootFS;
  this.apkEmbeddedLibrary = apkEmbeddedLibrary;
  this.FUNC_RE = /^([0-9a-fA-F]{8,16}) ([0-9a-fA-F]{8,16} )?[tTwW] (.*)$/;
};
inherits(UnixCppEntriesProvider, CppEntriesProvider);


UnixCppEntriesProvider.prototype.loadSymbols = function(libName) {
  this.parsePos = 0;
  if (this.apkEmbeddedLibrary && libName.endsWith('.apk')) {
    libName = this.apkEmbeddedLibrary;
  }
  if (this.targetRootFS) {
    libName = libName.substring(libName.lastIndexOf('/') + 1);
    libName = this.targetRootFS + libName;
  }
  try {
    this.symbols = [
      os.system(this.nmExec, ['-C', '-n', '-S', libName], -1, -1),
      os.system(this.nmExec, ['-C', '-n', '-S', '-D', libName], -1, -1)
    ];

    const objdumpOutput = os.system(this.objdumpExec, ['-h', libName], -1, -1);
    for (const line of objdumpOutput.split('\n')) {
      const [,sectionName,,vma,,fileOffset] = line.trim().split(/\s+/);
      if (sectionName === ".text") {
        this.fileOffsetMinusVma = parseInt(fileOffset, 16) - parseInt(vma, 16);
      }
    }
  } catch (e) {
    // If the library cannot be found on this system let's not panic.
    this.symbols = ['', ''];
  }
};


UnixCppEntriesProvider.prototype.parseNextLine = function() {
  if (this.symbols.length == 0) {
    return false;
  }
  var lineEndPos = this.symbols[0].indexOf('\n', this.parsePos);
  if (lineEndPos == -1) {
    this.symbols.shift();
    this.parsePos = 0;
    return this.parseNextLine();
  }

  var line = this.symbols[0].substring(this.parsePos, lineEndPos);
  this.parsePos = lineEndPos + 1;
  var fields = line.match(this.FUNC_RE);
  var funcInfo = null;
  if (fields) {
    funcInfo = { name: fields[3], start: parseInt(fields[1], 16) + this.fileOffsetMinusVma };
    if (fields[2]) {
      funcInfo.size = parseInt(fields[2], 16);
    }
  }
  return funcInfo;
};


function MacCppEntriesProvider(nmExec, objdumpExec, targetRootFS, apkEmbeddedLibrary) {
  UnixCppEntriesProvider.call(this, nmExec, objdumpExec, targetRootFS, apkEmbeddedLibrary);
  // Note an empty group. It is required, as UnixCppEntriesProvider expects 3 groups.
  this.FUNC_RE = /^([0-9a-fA-F]{8,16})() (.*)$/;
};
inherits(MacCppEntriesProvider, UnixCppEntriesProvider);


MacCppEntriesProvider.prototype.loadSymbols = function(libName) {
  this.parsePos = 0;
  libName = this.targetRootFS + libName;

  // It seems that in OS X `nm` thinks that `-f` is a format option, not a
  // "flat" display option flag.
  try {
    this.symbols = [os.system(this.nmExec, ['-n', libName], -1, -1), ''];
  } catch (e) {
    // If the library cannot be found on this system let's not panic.
    this.symbols = '';
  }
};


function WindowsCppEntriesProvider(_ignored_nmExec, _ignored_objdumpExec, targetRootFS,
                                   _ignored_apkEmbeddedLibrary) {
  this.targetRootFS = targetRootFS;
  this.symbols = '';
  this.parsePos = 0;
};
inherits(WindowsCppEntriesProvider, CppEntriesProvider);


WindowsCppEntriesProvider.FILENAME_RE = /^(.*)\.([^.]+)$/;


WindowsCppEntriesProvider.FUNC_RE =
    /^\s+0001:[0-9a-fA-F]{8}\s+([_\?@$0-9a-zA-Z]+)\s+([0-9a-fA-F]{8}).*$/;


WindowsCppEntriesProvider.IMAGE_BASE_RE =
    /^\s+0000:00000000\s+___ImageBase\s+([0-9a-fA-F]{8}).*$/;


// This is almost a constant on Windows.
WindowsCppEntriesProvider.EXE_IMAGE_BASE = 0x00400000;


WindowsCppEntriesProvider.prototype.loadSymbols = function(libName) {
  libName = this.targetRootFS + libName;
  var fileNameFields = libName.match(WindowsCppEntriesProvider.FILENAME_RE);
  if (!fileNameFields) return;
  var mapFileName = fileNameFields[1] + '.map';
  this.moduleType_ = fileNameFields[2].toLowerCase();
  try {
    this.symbols = read(mapFileName);
  } catch (e) {
    // If .map file cannot be found let's not panic.
    this.symbols = '';
  }
};


WindowsCppEntriesProvider.prototype.parseNextLine = function() {
  var lineEndPos = this.symbols.indexOf('\r\n', this.parsePos);
  if (lineEndPos == -1) {
    return false;
  }

  var line = this.symbols.substring(this.parsePos, lineEndPos);
  this.parsePos = lineEndPos + 2;

  // Image base entry is above all other symbols, so we can just
  // terminate parsing.
  var imageBaseFields = line.match(WindowsCppEntriesProvider.IMAGE_BASE_RE);
  if (imageBaseFields) {
    var imageBase = parseInt(imageBaseFields[1], 16);
    if ((this.moduleType_ == 'exe') !=
        (imageBase == WindowsCppEntriesProvider.EXE_IMAGE_BASE)) {
      return false;
    }
  }

  var fields = line.match(WindowsCppEntriesProvider.FUNC_RE);
  return fields ?
      { name: this.unmangleName(fields[1]), start: parseInt(fields[2], 16) } :
      null;
};


/**
 * Performs very simple unmangling of C++ names.
 *
 * Does not handle arguments and template arguments. The mangled names have
 * the form:
 *
 *   ?LookupInDescriptor@JSObject@internal@v8@@...arguments info...
 */
WindowsCppEntriesProvider.prototype.unmangleName = function(name) {
  // Empty or non-mangled name.
  if (name.length < 1 || name.charAt(0) != '?') return name;
  var nameEndPos = name.indexOf('@@');
  var components = name.substring(1, nameEndPos).split('@');
  components.reverse();
  return components.join('::');
};


class ArgumentsProcessor extends BaseArgumentsProcessor {
  getArgsDispatch() {
    let dispatch = {
      '-j': ['stateFilter', TickProcessor.VmStates.JS,
          'Show only ticks from JS VM state'],
      '-g': ['stateFilter', TickProcessor.VmStates.GC,
          'Show only ticks from GC VM state'],
      '-p': ['stateFilter', TickProcessor.VmStates.PARSER,
          'Show only ticks from PARSER VM state'],
      '-b': ['stateFilter', TickProcessor.VmStates.BYTECODE_COMPILER,
          'Show only ticks from BYTECODE_COMPILER VM state'],
      '-c': ['stateFilter', TickProcessor.VmStates.COMPILER,
          'Show only ticks from COMPILER VM state'],
      '-o': ['stateFilter', TickProcessor.VmStates.OTHER,
          'Show only ticks from OTHER VM state'],
      '-e': ['stateFilter', TickProcessor.VmStates.EXTERNAL,
          'Show only ticks from EXTERNAL VM state'],
      '--filter-runtime-timer': ['runtimeTimerFilter', null,
              'Show only ticks matching the given runtime timer scope'],
      '--call-graph-size': ['callGraphSize', TickProcessor.CALL_GRAPH_SIZE,
          'Set the call graph size'],
      '--ignore-unknown': ['ignoreUnknown', true,
          'Exclude ticks of unknown code entries from processing'],
      '--separate-ic': ['separateIc', parseBool,
          'Separate IC entries'],
      '--separate-bytecodes': ['separateBytecodes', parseBool,
          'Separate Bytecode entries'],
      '--separate-builtins': ['separateBuiltins', parseBool,
          'Separate Builtin entries'],
      '--separate-stubs': ['separateStubs', parseBool,
          'Separate Stub entries'],
      '--unix': ['platform', 'unix',
          'Specify that we are running on *nix platform'],
      '--windows': ['platform', 'windows',
          'Specify that we are running on Windows platform'],
      '--mac': ['platform', 'mac',
          'Specify that we are running on Mac OS X platform'],
      '--nm': ['nm', 'nm',
          'Specify the \'nm\' executable to use (e.g. --nm=/my_dir/nm)'],
      '--objdump': ['objdump', 'objdump',
          'Specify the \'objdump\' executable to use (e.g. --objdump=/my_dir/objdump)'],
      '--target': ['targetRootFS', '',
          'Specify the target root directory for cross environment'],
      '--apk-embedded-library': ['apkEmbeddedLibrary', '',
          'Specify the path of the embedded library for Android traces'],
      '--range': ['range', 'auto,auto',
          'Specify the range limit as [start],[end]'],
      '--distortion': ['distortion', 0,
          'Specify the logging overhead in picoseconds'],
      '--source-map': ['sourceMap', null,
          'Specify the source map that should be used for output'],
      '--timed-range': ['timedRange', true,
          'Ignore ticks before first and after last Date.now() call'],
      '--pairwise-timed-range': ['pairwiseTimedRange', true,
          'Ignore ticks outside pairs of Date.now() calls'],
      '--only-summary': ['onlySummary', true,
          'Print only tick summary, exclude other information'],
      '--preprocess': ['preprocessJson', true,
          'Preprocess for consumption with web interface']
    };
    dispatch['--js'] = dispatch['-j'];
    dispatch['--gc'] = dispatch['-g'];
    dispatch['--compiler'] = dispatch['-c'];
    dispatch['--other'] = dispatch['-o'];
    dispatch['--external'] = dispatch['-e'];
    dispatch['--ptr'] = dispatch['--pairwise-timed-range'];
    return dispatch;
  }

  getDefaultResults() {
    return {
      logFileName: 'v8.log',
      platform: 'unix',
      stateFilter: null,
      callGraphSize: 5,
      ignoreUnknown: false,
      separateIc: true,
      separateBytecodes: false,
      separateBuiltins: true,
      separateStubs: true,
      preprocessJson: null,
      targetRootFS: '',
      nm: 'nm',
      objdump: 'objdump',
      range: 'auto,auto',
      distortion: 0,
      timedRange: false,
      pairwiseTimedRange: false,
      onlySummary: false,
      runtimeTimerFilter: null,
    };
  }
}
