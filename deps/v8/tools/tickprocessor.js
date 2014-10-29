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


function V8Profile(separateIc) {
  Profile.call(this);
  if (!separateIc) {
    this.skipThisFunction = function(name) { return V8Profile.IC_RE.test(name); };
  }
};
inherits(V8Profile, Profile);


V8Profile.IC_RE =
    /^(?:CallIC|LoadIC|StoreIC)|(?:Builtin: (?:Keyed)?(?:Call|Load|Store)IC_)/;


/**
 * A thin wrapper around shell's 'read' function showing a file name on error.
 */
function readFile(fileName) {
  try {
    return read(fileName);
  } catch (e) {
    print(fileName + ': ' + (e.message || e));
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


function SnapshotLogProcessor() {
  LogReader.call(this, {
      'code-creation': {
          parsers: [null, parseInt, parseInt, parseInt, null, 'var-args'],
          processor: this.processCodeCreation },
      'code-move': { parsers: [parseInt, parseInt],
          processor: this.processCodeMove },
      'code-delete': { parsers: [parseInt],
          processor: this.processCodeDelete },
      'function-creation': null,
      'function-move': null,
      'function-delete': null,
      'sfi-move': null,
      'snapshot-pos': { parsers: [parseInt, parseInt],
          processor: this.processSnapshotPosition }});

  V8Profile.prototype.handleUnknownCode = function(operation, addr) {
    var op = Profile.Operation;
    switch (operation) {
      case op.MOVE:
        print('Snapshot: Code move event for unknown code: 0x' +
              addr.toString(16));
        break;
      case op.DELETE:
        print('Snapshot: Code delete event for unknown code: 0x' +
              addr.toString(16));
        break;
    }
  };

  this.profile_ = new V8Profile();
  this.serializedEntries_ = [];
}
inherits(SnapshotLogProcessor, LogReader);


SnapshotLogProcessor.prototype.processCodeCreation = function(
    type, kind, start, size, name, maybe_func) {
  if (maybe_func.length) {
    var funcAddr = parseInt(maybe_func[0]);
    var state = parseState(maybe_func[1]);
    this.profile_.addFuncCode(type, name, start, size, funcAddr, state);
  } else {
    this.profile_.addCode(type, name, start, size);
  }
};


SnapshotLogProcessor.prototype.processCodeMove = function(from, to) {
  this.profile_.moveCode(from, to);
};


SnapshotLogProcessor.prototype.processCodeDelete = function(start) {
  this.profile_.deleteCode(start);
};


SnapshotLogProcessor.prototype.processSnapshotPosition = function(addr, pos) {
  this.serializedEntries_[pos] = this.profile_.findEntry(addr);
};


SnapshotLogProcessor.prototype.processLogFile = function(fileName) {
  var contents = readFile(fileName);
  this.processLogChunk(contents);
};


SnapshotLogProcessor.prototype.getSerializedEntryName = function(pos) {
  var entry = this.serializedEntries_[pos];
  return entry ? entry.getRawName() : null;
};


function TickProcessor(
    cppEntriesProvider,
    separateIc,
    callGraphSize,
    ignoreUnknown,
    stateFilter,
    snapshotLogProcessor,
    distortion,
    range,
    sourceMap) {
  LogReader.call(this, {
      'shared-library': { parsers: [null, parseInt, parseInt],
          processor: this.processSharedLibrary },
      'code-creation': {
          parsers: [null, parseInt, parseInt, parseInt, null, 'var-args'],
          processor: this.processCodeCreation },
      'code-move': { parsers: [parseInt, parseInt],
          processor: this.processCodeMove },
      'code-delete': { parsers: [parseInt],
          processor: this.processCodeDelete },
      'sfi-move': { parsers: [parseInt, parseInt],
          processor: this.processFunctionMove },
      'snapshot-pos': { parsers: [parseInt, parseInt],
          processor: this.processSnapshotPosition },
      'tick': {
          parsers: [parseInt, parseInt, parseInt,
                    parseInt, parseInt, 'var-args'],
          processor: this.processTick },
      'heap-sample-begin': { parsers: [null, null, parseInt],
          processor: this.processHeapSampleBegin },
      'heap-sample-end': { parsers: [null, null],
          processor: this.processHeapSampleEnd },
      'timer-event-start' : { parsers: [null, null, null],
                              processor: this.advanceDistortion },
      'timer-event-end' : { parsers: [null, null, null],
                            processor: this.advanceDistortion },
      // Ignored events.
      'profiler': null,
      'function-creation': null,
      'function-move': null,
      'function-delete': null,
      'heap-sample-item': null,
      // Obsolete row types.
      'code-allocate': null,
      'begin-code-region': null,
      'end-code-region': null });

  this.cppEntriesProvider_ = cppEntriesProvider;
  this.callGraphSize_ = callGraphSize;
  this.ignoreUnknown_ = ignoreUnknown;
  this.stateFilter_ = stateFilter;
  this.snapshotLogProcessor_ = snapshotLogProcessor;
  this.sourceMap = sourceMap;
  this.deserializedEntriesNames_ = [];
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
        print('Code move event for unknown code: 0x' + addr.toString(16));
        break;
      case op.DELETE:
        print('Code delete event for unknown code: 0x' + addr.toString(16));
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

  this.profile_ = new V8Profile(separateIc);
  this.codeTypes_ = {};
  // Count each tick as a time unit.
  this.viewBuilder_ = new ViewBuilder(1);
  this.lastLogFileName_ = null;

  this.generation_ = 1;
  this.currentProducerProfile_ = null;
};
inherits(TickProcessor, LogReader);


TickProcessor.VmStates = {
  JS: 0,
  GC: 1,
  COMPILER: 2,
  OTHER: 3,
  EXTERNAL: 4,
  IDLE: 5
};


TickProcessor.CodeTypes = {
  CPP: 0,
  SHARED_LIB: 1
};
// Otherwise, this is JS-related code. We are not adding it to
// codeTypes_ map because there can be zillions of them.


TickProcessor.CALL_PROFILE_CUTOFF_PCT = 2.0;

TickProcessor.CALL_GRAPH_SIZE = 5;

/**
 * @override
 */
TickProcessor.prototype.printError = function(str) {
  print(str);
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
  return !(name in this.codeTypes_);
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
    name, startAddr, endAddr) {
  var entry = this.profile_.addLibrary(name, startAddr, endAddr);
  this.setCodeType(entry.getName(), 'SHARED_LIB');

  var self = this;
  var libFuncs = this.cppEntriesProvider_.parseVmSymbols(
      name, startAddr, endAddr, function(fName, fStart, fEnd) {
    self.profile_.addStaticCode(fName, fStart, fEnd);
    self.setCodeType(fName, 'CPP');
  });
};


TickProcessor.prototype.processCodeCreation = function(
    type, kind, start, size, name, maybe_func) {
  name = this.deserializedEntriesNames_[start] || name;
  if (maybe_func.length) {
    var funcAddr = parseInt(maybe_func[0]);
    var state = parseState(maybe_func[1]);
    this.profile_.addFuncCode(type, name, start, size, funcAddr, state);
  } else {
    this.profile_.addCode(type, name, start, size);
  }
};


TickProcessor.prototype.processCodeMove = function(from, to) {
  this.profile_.moveCode(from, to);
};


TickProcessor.prototype.processCodeDelete = function(start) {
  this.profile_.deleteCode(start);
};


TickProcessor.prototype.processFunctionMove = function(from, to) {
  this.profile_.moveFunc(from, to);
};


TickProcessor.prototype.processSnapshotPosition = function(addr, pos) {
  if (this.snapshotLogProcessor_) {
    this.deserializedEntriesNames_[addr] =
      this.snapshotLogProcessor_.getSerializedEntryName(pos);
  }
};


TickProcessor.prototype.includeTick = function(vmState) {
  return this.stateFilter_ == null || this.stateFilter_ == vmState;
};

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

  this.profile_.recordTick(this.processStack(pc, tos_or_external_callback, stack));
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

  // Count library ticks
  var flatViewNodes = flatView.head.children;
  var self = this;

  var libraryTicks = 0;
  this.printHeader('Shared libraries');
  this.printEntries(flatViewNodes, totalTicks, null,
      function(name) { return self.isSharedLibrary(name); },
      function(rec) { libraryTicks += rec.selfTime; });
  var nonLibraryTicks = totalTicks - libraryTicks;

  var jsTicks = 0;
  this.printHeader('JavaScript');
  this.printEntries(flatViewNodes, totalTicks, nonLibraryTicks,
      function(name) { return self.isJsCode(name); },
      function(rec) { jsTicks += rec.selfTime; });

  var cppTicks = 0;
  this.printHeader('C++');
  this.printEntries(flatViewNodes, totalTicks, nonLibraryTicks,
      function(name) { return self.isCppCode(name); },
      function(rec) { cppTicks += rec.selfTime; });

  this.printHeader('Summary');
  this.printLine('JavaScript', jsTicks, totalTicks, nonLibraryTicks);
  this.printLine('C++', cppTicks, totalTicks, nonLibraryTicks);
  this.printLine('GC', this.ticks_.gc, totalTicks, nonLibraryTicks);
  this.printLine('Shared libraries', libraryTicks, totalTicks, null);
  if (!this.ignoreUnknown_ && this.ticks_.unaccounted > 0) {
    this.printLine('Unaccounted', this.ticks_.unaccounted,
                   this.ticks_.total, null);
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
    profile, totalTicks, nonLibTicks, filterP, callback) {
  var that = this;
  this.processProfile(profile, filterP, function (rec) {
    if (rec.selfTime == 0) return;
    callback(rec);
    var funcName = that.formatFunctionName(rec.internalFuncName);
    that.printLine(funcName, rec.selfTime, totalTicks, nonLibTicks);
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
    libName, libStart, libEnd, processorFunc) {
  this.loadSymbols(libName);

  var prevEntry;

  function addEntry(funcInfo) {
    // Several functions can be mapped onto the same address. To avoid
    // creating zero-sized entries, skip such duplicates.
    // Also double-check that function belongs to the library address space.
    if (prevEntry && !prevEntry.end &&
        prevEntry.start < funcInfo.start &&
        prevEntry.start >= libStart && funcInfo.start <= libEnd) {
      processorFunc(prevEntry.name, prevEntry.start, funcInfo.start);
    }
    if (funcInfo.end &&
        (!prevEntry || prevEntry.start != funcInfo.start) &&
        funcInfo.start >= libStart && funcInfo.end <= libEnd) {
      processorFunc(funcInfo.name, funcInfo.start, funcInfo.end);
    }
    prevEntry = funcInfo;
  }

  while (true) {
    var funcInfo = this.parseNextLine();
    if (funcInfo === null) {
      continue;
    } else if (funcInfo === false) {
      break;
    }
    if (funcInfo.start < libStart && funcInfo.start < libEnd - libStart) {
      funcInfo.start += libStart;
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


function UnixCppEntriesProvider(nmExec, targetRootFS) {
  this.symbols = [];
  this.parsePos = 0;
  this.nmExec = nmExec;
  this.targetRootFS = targetRootFS;
  this.FUNC_RE = /^([0-9a-fA-F]{8,16}) ([0-9a-fA-F]{8,16} )?[tTwW] (.*)$/;
};
inherits(UnixCppEntriesProvider, CppEntriesProvider);


UnixCppEntriesProvider.prototype.loadSymbols = function(libName) {
  this.parsePos = 0;
  libName = this.targetRootFS + libName;
  try {
    this.symbols = [
      os.system(this.nmExec, ['-C', '-n', '-S', libName], -1, -1),
      os.system(this.nmExec, ['-C', '-n', '-S', '-D', libName], -1, -1)
    ];
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
    funcInfo = { name: fields[3], start: parseInt(fields[1], 16) };
    if (fields[2]) {
      funcInfo.size = parseInt(fields[2], 16);
    }
  }
  return funcInfo;
};


function MacCppEntriesProvider(nmExec, targetRootFS) {
  UnixCppEntriesProvider.call(this, nmExec, targetRootFS);
  // Note an empty group. It is required, as UnixCppEntriesProvider expects 3 groups.
  this.FUNC_RE = /^([0-9a-fA-F]{8,16}) ()[iItT] (.*)$/;
};
inherits(MacCppEntriesProvider, UnixCppEntriesProvider);


MacCppEntriesProvider.prototype.loadSymbols = function(libName) {
  this.parsePos = 0;
  libName = this.targetRootFS + libName;
  try {
    this.symbols = [os.system(this.nmExec, ['-n', '-f', libName], -1, -1), ''];
  } catch (e) {
    // If the library cannot be found on this system let's not panic.
    this.symbols = '';
  }
};


function WindowsCppEntriesProvider(_ignored_nmExec, targetRootFS) {
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


function ArgumentsProcessor(args) {
  this.args_ = args;
  this.result_ = ArgumentsProcessor.DEFAULTS;

  this.argsDispatch_ = {
    '-j': ['stateFilter', TickProcessor.VmStates.JS,
        'Show only ticks from JS VM state'],
    '-g': ['stateFilter', TickProcessor.VmStates.GC,
        'Show only ticks from GC VM state'],
    '-c': ['stateFilter', TickProcessor.VmStates.COMPILER,
        'Show only ticks from COMPILER VM state'],
    '-o': ['stateFilter', TickProcessor.VmStates.OTHER,
        'Show only ticks from OTHER VM state'],
    '-e': ['stateFilter', TickProcessor.VmStates.EXTERNAL,
        'Show only ticks from EXTERNAL VM state'],
    '--call-graph-size': ['callGraphSize', TickProcessor.CALL_GRAPH_SIZE,
        'Set the call graph size'],
    '--ignore-unknown': ['ignoreUnknown', true,
        'Exclude ticks of unknown code entries from processing'],
    '--separate-ic': ['separateIc', true,
        'Separate IC entries'],
    '--unix': ['platform', 'unix',
        'Specify that we are running on *nix platform'],
    '--windows': ['platform', 'windows',
        'Specify that we are running on Windows platform'],
    '--mac': ['platform', 'mac',
        'Specify that we are running on Mac OS X platform'],
    '--nm': ['nm', 'nm',
        'Specify the \'nm\' executable to use (e.g. --nm=/my_dir/nm)'],
    '--target': ['targetRootFS', '',
        'Specify the target root directory for cross environment'],
    '--snapshot-log': ['snapshotLogFileName', 'snapshot.log',
        'Specify snapshot log file to use (e.g. --snapshot-log=snapshot.log)'],
    '--range': ['range', 'auto,auto',
        'Specify the range limit as [start],[end]'],
    '--distortion': ['distortion', 0,
        'Specify the logging overhead in picoseconds'],
    '--source-map': ['sourceMap', null,
        'Specify the source map that should be used for output']
  };
  this.argsDispatch_['--js'] = this.argsDispatch_['-j'];
  this.argsDispatch_['--gc'] = this.argsDispatch_['-g'];
  this.argsDispatch_['--compiler'] = this.argsDispatch_['-c'];
  this.argsDispatch_['--other'] = this.argsDispatch_['-o'];
  this.argsDispatch_['--external'] = this.argsDispatch_['-e'];
};


ArgumentsProcessor.DEFAULTS = {
  logFileName: 'v8.log',
  snapshotLogFileName: null,
  platform: 'unix',
  stateFilter: null,
  callGraphSize: 5,
  ignoreUnknown: false,
  separateIc: false,
  targetRootFS: '',
  nm: 'nm',
  range: 'auto,auto',
  distortion: 0
};


ArgumentsProcessor.prototype.parse = function() {
  while (this.args_.length) {
    var arg = this.args_[0];
    if (arg.charAt(0) != '-') {
      break;
    }
    this.args_.shift();
    var userValue = null;
    var eqPos = arg.indexOf('=');
    if (eqPos != -1) {
      userValue = arg.substr(eqPos + 1);
      arg = arg.substr(0, eqPos);
    }
    if (arg in this.argsDispatch_) {
      var dispatch = this.argsDispatch_[arg];
      this.result_[dispatch[0]] = userValue == null ? dispatch[1] : userValue;
    } else {
      return false;
    }
  }

  if (this.args_.length >= 1) {
      this.result_.logFileName = this.args_.shift();
  }
  return true;
};


ArgumentsProcessor.prototype.result = function() {
  return this.result_;
};


ArgumentsProcessor.prototype.printUsageAndExit = function() {

  function padRight(s, len) {
    s = s.toString();
    if (s.length < len) {
      s = s + (new Array(len - s.length + 1).join(' '));
    }
    return s;
  }

  print('Cmdline args: [options] [log-file-name]\n' +
        'Default log file name is "' +
        ArgumentsProcessor.DEFAULTS.logFileName + '".\n');
  print('Options:');
  for (var arg in this.argsDispatch_) {
    var synonims = [arg];
    var dispatch = this.argsDispatch_[arg];
    for (var synArg in this.argsDispatch_) {
      if (arg !== synArg && dispatch === this.argsDispatch_[synArg]) {
        synonims.push(synArg);
        delete this.argsDispatch_[synArg];
      }
    }
    print('  ' + padRight(synonims.join(', '), 20) + dispatch[2]);
  }
  quit(2);
};
