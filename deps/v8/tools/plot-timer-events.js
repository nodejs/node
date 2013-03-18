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

var kV8BinarySuffixes = ["/d8", "/libv8.so"];
var kStackFrames = 8;

var kTimerEventWidth = 0.33;
var kExecutionFrameWidth = 0.2;
var kStackFrameWidth = 0.1;
var kGapWidth = 0.05;

var kY1Offset = 10;

var kResX = 1600;
var kResY = 600;
var kPauseLabelPadding = 5;
var kNumPauseLabels = 7;
var kTickHalfDuration = 0.5;  // Milliseconds
var kCodeKindLabelPadding = 100;
var kMinRangeLength = 0.0005;  // Milliseconds

var num_timer_event = kY1Offset + 0.5;

var kNumThreads = 2;
var kExecutionThreadId = 0;

function assert(something, message) {
  if (!something) {
    print(new Error(message).stack);
  }
}

function TimerEvent(color, pause, thread_id) {
  assert(thread_id >= 0 && thread_id < kNumThreads, "invalid thread id");
  this.color = color;
  this.pause = pause;
  this.ranges = [];
  this.thread_id = thread_id;
  this.index = ++num_timer_event;
}


var TimerEvents = {
  'V8.Execute':              new TimerEvent("#000000", false, 0),
  'V8.External':             new TimerEvent("#3399FF", false, 0),
  'V8.CompileFullCode':      new TimerEvent("#CC0000",  true, 0),
  'V8.RecompileSynchronous': new TimerEvent("#CC0044",  true, 0),
  'V8.RecompileParallel':    new TimerEvent("#CC4499", false, 1),
  'V8.CompileEval':          new TimerEvent("#CC4400",  true, 0),
  'V8.Parse':                new TimerEvent("#00CC00",  true, 0),
  'V8.PreParse':             new TimerEvent("#44CC00",  true, 0),
  'V8.ParseLazy':            new TimerEvent("#00CC44",  true, 0),
  'V8.GCScavenger':          new TimerEvent("#0044CC",  true, 0),
  'V8.GCCompactor':          new TimerEvent("#4444CC",  true, 0),
  'V8.GCContext':            new TimerEvent("#4400CC",  true, 0),
}


Array.prototype.top = function() {
  if (this.length == 0) return undefined;
  return this[this.length - 1];
}

var event_stack = [];
var last_time_stamp = [];

for (var i = 0; i < kNumThreads; i++) {
  event_stack[i] = [];
  last_time_stamp[i] = -1;
}


function CodeKind(color, kinds) {
  this.color = color;
  this.in_execution = [];
  this.stack_frames = [];
  for (var i = 0; i < kStackFrames; i++) this.stack_frames.push([]);
  this.kinds = kinds;
}


var CodeKinds = {
  'external ': new CodeKind("#3399FF", [-3]),
  'reg.exp. ': new CodeKind("#0000FF", [-2]),
  'runtime  ': new CodeKind("#000000", [-1]),
  'full code': new CodeKind("#DD0000", [0]),
  'opt code ': new CodeKind("#00EE00", [1]),
  'code stub': new CodeKind("#FF00FF", [2]),
  'built-in ': new CodeKind("#AA00AA", [3]),
  'inl.cache': new CodeKind("#4444AA", [4, 5, 6, 7, 8, 9, 10, 11, 12, 13]),
}


var xrange_start;
var xrange_end;
var obj_index = 0;
var execution_pauses = [];
var code_map = new CodeMap();

var xrange_start_override = undefined;
var xrange_end_override = undefined;
var distortion_per_entry = 0.005;  // Milliseconds
var pause_tolerance = 0.005;  // Milliseconds.

var distortion = 0;


function Range(start, end) {
  // Everthing from here are in milliseconds.
  this.start = start;
  this.end = end;
}


function Tick(tick) {
  this.tick = tick;
}


Range.prototype.duration = function() { return this.end - this.start; }


function ProcessTimerEventStart(name, start) {
  // Find out the thread id.
  var new_event = TimerEvents[name];
  if (new_event === undefined) return;
  var thread_id = new_event.thread_id;

  start = Math.max(last_time_stamp[thread_id] + kMinRangeLength, start);

  // Last event on this thread is done with the start of this event.
  var last_event = event_stack[thread_id].top();
  if (last_event !== undefined) {
    var new_range = new Range(last_time_stamp[thread_id], start);
    last_event.ranges.push(new_range);
  }
  event_stack[thread_id].push(new_event);
  last_time_stamp[thread_id] = start;
}


function ProcessTimerEventEnd(name, end) {
  // Find out about the thread_id.
  var finished_event = TimerEvents[name];
  var thread_id = finished_event.thread_id;
  assert(finished_event === event_stack[thread_id].pop(),
         "inconsistent event stack");

  end = Math.max(last_time_stamp[thread_id] + kMinRangeLength, end);

  var new_range = new Range(last_time_stamp[thread_id], end);
  finished_event.ranges.push(new_range);
  last_time_stamp[thread_id] = end;
}


function ProcessCodeCreateEvent(type, kind, address, size, name) {
  var code_entry = new CodeMap.CodeEntry(size, name);
  code_entry.kind = kind;
  code_map.addCode(address, code_entry);
}


function ProcessCodeMoveEvent(from, to) {
  code_map.moveCode(from, to);
}


function ProcessCodeDeleteEvent(address) {
  code_map.deleteCode(address);
}


function ProcessSharedLibrary(name, start, end) {
  var code_entry = new CodeMap.CodeEntry(end - start, name);
  code_entry.kind = -3;  // External code kind.
  for (var i = 0; i < kV8BinarySuffixes.length; i++) {
    var suffix = kV8BinarySuffixes[i];
    if (name.indexOf(suffix, name.length - suffix.length) >= 0) {
      code_entry.kind = -1;  // V8 runtime code kind.
      break;
    }
  }
  code_map.addLibrary(start, code_entry);
}


function FindCodeKind(kind) {
  for (name in CodeKinds) {
    if (CodeKinds[name].kinds.indexOf(kind) >= 0) {
      return CodeKinds[name];
    }
  }
}


function ProcessTickEvent(pc, sp, timer, unused_x, unused_y, vmstate, stack) {
  var tick = new Tick(timer);

  var entry = code_map.findEntry(pc);
  if (entry) FindCodeKind(entry.kind).in_execution.push(tick);

  for (var i = 0; i < kStackFrames; i++) {
    if (!stack[i]) break;
    var entry = code_map.findEntry(stack[i]);
    if (entry) FindCodeKind(entry.kind).stack_frames[i].push(tick);
  }
}


function FindPlotRange() {
  var start_found = (xrange_start_override || xrange_start_override == 0);
  var end_found = (xrange_end_override || xrange_end_override == 0);
  xrange_start = start_found ? xrange_start_override : Infinity;
  xrange_end = end_found ? xrange_end_override : -Infinity;

  if (start_found && end_found) return;

  for (name in TimerEvents) {
    var ranges = TimerEvents[name].ranges;
    for (var i = 0; i < ranges.length; i++) {
      if (ranges[i].start < xrange_start && !start_found) {
        xrange_start = ranges[i].start;
      }
      if (ranges[i].end > xrange_end && !end_found) {
        xrange_end = ranges[i].end;
      }
    }
  }

  for (codekind in CodeKinds) {
    var ticks = CodeKinds[codekind].in_execution;
    for (var i = 0; i < ticks.length; i++) {
      if (ticks[i].tick < xrange_start && !start_found) {
        xrange_start = ticks[i].tick;
      }
      if (ticks[i].tick > xrange_end && !end_found) {
        xrange_end = ticks[i].tick;
      }
    }
  }

  // Set pause tolerance to something appropriate for the plot resolution
  // to make it easier for gnuplot.
  pause_tolerance = (xrange_end - xrange_start) / kResX / 10;
}


function parseTimeStamp(timestamp) {
  distortion += distortion_per_entry;
  return parseInt(timestamp) / 1000 - distortion;
}


function ParseArguments(args) {
  var processor = new ArgumentsProcessor(args);
  do {
    if (!processor.parse()) break;
    var result = processor.result();
    var distortion = parseInt(result.distortion);
    if (isNaN(distortion)) break;
    // Convert picoseconds to milliseconds.
    distortion_per_entry = distortion / 1000000;
    var rangelimits = result.range.split(",");
    var range_start = parseInt(rangelimits[0]);
    var range_end = parseInt(rangelimits[1]);
    xrange_start_override = isNaN(range_start) ? undefined : range_start;
    xrange_end_override = isNaN(range_end) ? undefined : range_end;
    return;
  } while (false);
  processor.printUsageAndExit();
}


function CollectData() {
  // Collect data from log.
  var logreader = new LogReader(
    { 'timer-event-start': { parsers: [null, parseTimeStamp],
                             processor: ProcessTimerEventStart },
      'timer-event-end':   { parsers: [null, parseTimeStamp],
                             processor: ProcessTimerEventEnd },
      'shared-library': { parsers: [null, parseInt, parseInt],
                          processor: ProcessSharedLibrary },
      'code-creation':  { parsers: [null, parseInt, parseInt, parseInt, null],
                          processor: ProcessCodeCreateEvent },
      'code-move':      { parsers: [parseInt, parseInt],
                          processor: ProcessCodeMoveEvent },
      'code-delete':    { parsers: [parseInt],
                          processor: ProcessCodeDeleteEvent },
      'tick':           { parsers: [parseInt, parseInt, parseTimeStamp,
                                    null, null, parseInt, 'var-args'],
                          processor: ProcessTickEvent }
    });

  var line;
  while (line = readline()) {
    logreader.processLogLine(line);
  }

  // Collect execution pauses.
  for (name in TimerEvents) {
    var event = TimerEvents[name];
    if (!event.pause) continue;
    var ranges = event.ranges;
    for (var j = 0; j < ranges.length; j++) execution_pauses.push(ranges[j]);
  }
  execution_pauses = MergeRanges(execution_pauses);
}


function DrawBar(row, color, start, end, width) {
  obj_index++;
  command = "set object " + obj_index + " rect";
  command += " from " + start + ", " + (row - width);
  command += " to " + end + ", " + (row + width);
  command += " fc rgb \"" + color + "\"";
  print(command);
}


function TicksToRanges(ticks) {
  var ranges = [];
  for (var i = 0; i < ticks.length; i++) {
    var tick = ticks[i].tick;
    ranges.push(new Range(tick - kTickHalfDuration, tick + kTickHalfDuration));
  }
  return ranges;
}


function MergeRanges(ranges) {
  ranges.sort(function(a, b) { return a.start - b.start; });
  var result = [];
  var j = 0;
  for (var i = 0; i < ranges.length; i = j) {
    var merge_start = ranges[i].start;
    if (merge_start > xrange_end) break;  // Out of plot range.
    var merge_end = ranges[i].end;
    for (j = i + 1; j < ranges.length; j++) {
      var next_range = ranges[j];
      // Don't merge ranges if there is no overlap (including merge tolerance).
      if (next_range.start > merge_end + pause_tolerance) break;
      // Merge ranges.
      if (next_range.end > merge_end) {  // Extend range end.
        merge_end = next_range.end;
      }
    }
    if (merge_end < xrange_start) continue;  // Out of plot range.
    if (merge_end < merge_start) continue;  // Not an actual range.
    result.push(new Range(merge_start, merge_end));
  }
  return result;
}


function RestrictRangesTo(ranges, start, end) {
  var result = [];
  for (var i = 0; i < ranges.length; i++) {
    if (ranges[i].start <= end && ranges[i].end >= start) {
      result.push(new Range(Math.max(ranges[i].start, start),
                            Math.min(ranges[i].end, end)));
    }
  }
  return result;
}


function GnuplotOutput() {
  print("set terminal pngcairo size " + kResX + "," + kResY +
        " enhanced font 'Helvetica,10'");
  print("set yrange [0:" + (num_timer_event + 1) + "]");
  print("set xlabel \"execution time in ms\"");
  print("set xrange [" + xrange_start + ":" + xrange_end + "]");
  print("set style fill pattern 2 bo 1");
  print("set style rect fs solid 1 noborder");
  print("set style line 1 lt 1 lw 1 lc rgb \"#000000\"");
  print("set xtics out nomirror");
  print("unset key");

  var percentages = {};
  var total = 0;
  for (var name in TimerEvents) {
    var event = TimerEvents[name];
    var ranges = RestrictRangesTo(event.ranges, xrange_start, xrange_end);
    ranges = MergeRanges(ranges);
    var sum =
      ranges.map(function(range) { return range.duration(); })
          .reduce(function(a, b) { return a + b; }, 0);
    percentages[name] = (sum / (xrange_end - xrange_start) * 100).toFixed(1);
  }

  // Name Y-axis.
  var ytics = [];
  for (name in TimerEvents) {
    var index = TimerEvents[name].index;
    ytics.push('"' + name + ' (' + percentages[name] + '%%)" ' + index);
  }
  ytics.push('"code kind being executed"' + ' ' + (kY1Offset - 1));
  ytics.push('"top ' + kStackFrames + ' js stack frames"' + ' ' +
             (kY1Offset - 2));
  ytics.push('"pause times" 0');
  print("set ytics out nomirror (" + ytics.join(', ') + ")");

  // Plot timeline.
  for (var name in TimerEvents) {
    var event = TimerEvents[name];
    var ranges = MergeRanges(event.ranges);
    for (var i = 0; i < ranges.length; i++) {
      DrawBar(event.index, event.color,
              ranges[i].start, ranges[i].end,
              kTimerEventWidth);
    }
  }

  // Plot code kind gathered from ticks.
  for (var name in CodeKinds) {
    var code_kind = CodeKinds[name];
    var offset = kY1Offset - 1;
    // Top most frame.
    var row = MergeRanges(TicksToRanges(code_kind.in_execution));
    for (var j = 0; j < row.length; j++) {
      DrawBar(offset, code_kind.color,
              row[j].start, row[j].end, kExecutionFrameWidth);
    }
    offset = offset - 2 * kExecutionFrameWidth - kGapWidth;
    // Javascript frames.
    for (var i = 0; i < kStackFrames; i++) {
      offset = offset - 2 * kStackFrameWidth - kGapWidth;
      row = MergeRanges(TicksToRanges(code_kind.stack_frames[i]));
      for (var j = 0; j < row.length; j++) {
        DrawBar(offset, code_kind.color,
                row[j].start, row[j].end, kStackFrameWidth);
      }
    }
  }

  // Add labels as legend for code kind colors.
  var padding = kCodeKindLabelPadding * (xrange_end - xrange_start) / kResX;
  var label_x = xrange_start;
  var label_y = kY1Offset;
  for (var name in CodeKinds) {
    label_x += padding;
    print("set label \"" + name + "\" at " + label_x + "," + label_y +
          " textcolor rgb \"" + CodeKinds[name].color + "\"" +
          " font \"Helvetica,9'\"");
  }

  if (execution_pauses.length == 0) {
    // Force plot and return without plotting execution pause impulses.
    print("plot 1/0");
    return;
  }

  // Label the longest pauses.
  execution_pauses.sort(
      function(a, b) { return b.duration() - a.duration(); });

  var max_pause_time = execution_pauses[0].duration();
  padding = kPauseLabelPadding * (xrange_end - xrange_start) / kResX;
  var y_scale = kY1Offset / max_pause_time / 2;
  for (var i = 0; i < execution_pauses.length && i < kNumPauseLabels; i++) {
    var pause = execution_pauses[i];
    var label_content = (pause.duration() | 0) + " ms";
    var label_x = pause.end + padding;
    var label_y = Math.max(1, (pause.duration() * y_scale));
    print("set label \"" + label_content + "\" at " +
          label_x + "," + label_y + " font \"Helvetica,7'\"");
  }

  // Scale second Y-axis appropriately.
  var y2range = max_pause_time * num_timer_event / kY1Offset * 2;
  print("set y2range [0:" + y2range + "]");
  // Plot graph with impulses as data set.
  print("plot '-' using 1:2 axes x1y2 with impulses ls 1");
  for (var i = 0; i < execution_pauses.length; i++) {
    var pause = execution_pauses[i];
    print(pause.end + " " + pause.duration());
  }
  print("e");
}


ParseArguments(arguments);
CollectData();
FindPlotRange();
GnuplotOutput();
