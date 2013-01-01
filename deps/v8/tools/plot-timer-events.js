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

var kPauseTolerance = 0.1;  // Milliseconds.
var kY1Offset = 10;

var kResX = 1600;
var kResY = 600;
var kPauseLabelPadding = 5;
var kNumPauseLabels = 7;
var kTickHalfDuration = 0.5;  // Milliseconds
var kCodeKindLabelPadding = 100;

var num_timer_event = kY1Offset + 0.5;


function TimerEvent(color, pause, no_execution) {
  this.color = color;
  this.pause = pause;
  this.ranges = [];
  this.no_execution = no_execution;
  this.index = ++num_timer_event;
}


var TimerEvents = {
  'V8.Execute':              new TimerEvent("#000000", false, false),
  'V8.External':             new TimerEvent("#3399FF", false,  true),
  'V8.CompileFullCode':      new TimerEvent("#CC0000",  true,  true),
  'V8.RecompileSynchronous': new TimerEvent("#CC0044",  true,  true),
  'V8.RecompileParallel':    new TimerEvent("#CC4499", false, false),
  'V8.CompileEval':          new TimerEvent("#CC4400",  true,  true),
  'V8.Parse':                new TimerEvent("#00CC00",  true,  true),
  'V8.PreParse':             new TimerEvent("#44CC00",  true,  true),
  'V8.ParseLazy':            new TimerEvent("#00CC44",  true,  true),
  'V8.GCScavenger':          new TimerEvent("#0044CC",  true,  true),
  'V8.GCCompactor':          new TimerEvent("#4444CC",  true,  true),
  'V8.GCContext':            new TimerEvent("#4400CC",  true,  true),
}

var kExecutionEvent = TimerEvents['V8.Execute'];


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


var xrange_start = Infinity;
var xrange_end = 0;
var obj_index = 0;
var execution_pauses = [];
var code_map = new CodeMap();

var xrange_start_override = undefined;
var xrange_end_override = undefined;
var distortion_per_entry = 0.005;  // Milliseconds

var sort_by_start = [];
var sort_by_end = [];
var sorted_ticks = [];


function Range(start, end) {
  // Everthing from here are in milliseconds.
  this.start = start;
  this.end = end;
}


function Tick(tick) {
  this.tick = tick;
}


Range.prototype.duration = function() { return this.end - this.start; }


function ProcessTimerEvent(name, start, length) {
  var event = TimerEvents[name];
  if (event === undefined) return;
  start /= 1000;  // Convert to milliseconds.
  length /= 1000;
  var end = start + length;
  var range = new Range(start, end);
  event.ranges.push(range);
  sort_by_start.push(range);
  sort_by_end.push(range);
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
  timer /= 1000;
  var tick = new Tick(timer);

  var entered = false;
  var entry = code_map.findEntry(pc);
  if (entry) {
    FindCodeKind(entry.kind).in_execution.push(tick);
    entered = true;
  }

  for (var i = 0; i < kStackFrames; i++) {
    if (!stack[i]) break;
    var entry = code_map.findEntry(stack[i]);
    if (entry) {
      FindCodeKind(entry.kind).stack_frames[i].push(tick);
      entered = true;
    }
  }

  if (entered) sorted_ticks.push(tick);
}


function ProcessDistortion(distortion_in_picoseconds) {
  distortion_per_entry = distortion_in_picoseconds / 1000000;
}


function ProcessPlotRange(start, end) {
  xrange_start_override = start;
  xrange_end_override = end;
}


function Undistort() {
  // Undistort timers wrt instrumentation overhead.
  sort_by_start.sort(function(a, b) { return b.start - a.start; });
  sort_by_end.sort(function(a, b) { return b.end - a.end; });
  sorted_ticks.sort(function(a, b) { return b.tick - a.tick; });
  var distortion = 0;

  var next_start = sort_by_start.pop();
  var next_end = sort_by_end.pop();
  var next_tick = sorted_ticks.pop();

  function UndistortTicksUntil(tick) {
    while (next_tick) {
      if (next_tick.tick > tick) return;
      next_tick.tick -= distortion;
      next_tick = sorted_ticks.pop();
    }
  }

  while (true) {
    var next_start_start = next_start ? next_start.start : Infinity;
    var next_end_end = next_end ? next_end.end : Infinity;
    if (!next_start && !next_end) {
      UndistortTicksUntil(Infinity);
      break;
    }
    if (next_start_start <= next_end_end) {
      UndistortTicksUntil(next_start_start);
      // Undistort the start time stamp.
      next_start.start -= distortion;
      next_start = sort_by_start.pop();
    } else {
      // Undistort the end time stamp.  We completely attribute the overhead
      // to the point when we stop and log the timer, so we increase the
      // distortion only here.
      UndistortTicksUntil(next_end_end);
      next_end.end -= distortion;
      distortion += distortion_per_entry;
      next_end = sort_by_end.pop();
    }
  }

  sort_by_start = undefined;
  sort_by_end = undefined;
  sorted_ticks = undefined;

  // Make sure that start <= end applies for every range.
  for (name in TimerEvents) {
    var ranges = TimerEvents[name].ranges;
    for (var j = 0; j < ranges.length; j++) {
      if (ranges[j].end < ranges[j].start) ranges[j].end = ranges[j].start;
    }
  }
}


function CollectData() {
  // Collect data from log.
  var logreader = new LogReader(
    { 'timer-event' :   { parsers: [null, parseInt, parseInt],
                          processor: ProcessTimerEvent },
      'shared-library': { parsers: [null, parseInt, parseInt],
                          processor: ProcessSharedLibrary },
      'code-creation':  { parsers: [null, parseInt, parseInt, parseInt, null],
                          processor: ProcessCodeCreateEvent },
      'code-move':      { parsers: [parseInt, parseInt],
                          processor: ProcessCodeMoveEvent },
      'code-delete':    { parsers: [parseInt],
                          processor: ProcessCodeDeleteEvent },
      'tick':           { parsers: [parseInt, parseInt, parseInt,
                                    null, null, parseInt, 'var-args'],
                          processor: ProcessTickEvent },
      'distortion':     { parsers: [parseInt],
                          processor: ProcessDistortion },
      'plot-range':     { parsers: [parseInt, parseInt],
                          processor: ProcessPlotRange },
    });

  var line;
  while (line = readline()) {
    logreader.processLogLine(line);
  }

  Undistort();

  // Figure out plot range.
  var execution_ranges = kExecutionEvent.ranges;
  for (var i = 0; i < execution_ranges.length; i++) {
    if (execution_ranges[i].start < xrange_start) {
      xrange_start = execution_ranges[i].start;
    }
    if (execution_ranges[i].end > xrange_end) {
      xrange_end = execution_ranges[i].end;
    }
  }

  // Collect execution pauses.
  for (name in TimerEvents) {
    var event = TimerEvents[name];
    if (!event.pause) continue;
    var ranges = event.ranges;
    for (var j = 0; j < ranges.length; j++) execution_pauses.push(ranges[j]);
  }
  execution_pauses = MergeRanges(execution_pauses);

  // Knock out time not spent in javascript execution.  Note that this also
  // includes time spent external code, which do not contribute to execution
  // pauses.
  var exclude_ranges = [];
  for (name in TimerEvents) {
    var event = TimerEvents[name];
    if (!event.no_execution) continue;
    var ranges = event.ranges;
    // Add ranges of this event to the pause list.
    for (var j = 0; j < ranges.length; j++) {
      exclude_ranges.push(ranges[j]);
    }
  }

  kExecutionEvent.ranges = MergeRanges(kExecutionEvent.ranges);
  exclude_ranges = MergeRanges(exclude_ranges);
  kExecutionEvent.ranges = ExcludeRanges(kExecutionEvent.ranges,
                                         exclude_ranges);
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
      if (next_range.start > merge_end + kPauseTolerance) break;
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


function ExcludeRanges(include, exclude) {
  // We assume that both input lists are sorted and merged with MergeRanges.
  var result = [];
  var exclude_index = 0;
  var include_index = 0;
  var include_start, include_end, exclude_start, exclude_end;

  function NextInclude() {
    if (include_index >= include.length) return false;
    include_start = include[include_index].start;
    include_end = include[include_index].end;
    include_index++;
    return true;
  }

  function NextExclude() {
    if (exclude_index >= exclude.length) {
      // No more exclude, finish by repeating case (2).
      exclude_start = Infinity;
      exclude_end = Infinity;
      return false;
    }
    exclude_start = exclude[exclude_index].start;
    exclude_end = exclude[exclude_index].end;
    exclude_index++;
    return true;
  }

  if (!NextInclude() || !NextExclude()) return include;

  while (true) {
    if (exclude_end <= include_start) {
      // (1) Exclude and include do not overlap.
      // Include       #####
      // Exclude   ##
      NextExclude();
    } else if (include_end <= exclude_start) {
      // (2) Exclude and include do not overlap.
      // Include   #####
      // Exclude         ###
      result.push(new Range(include_start, include_end));
      if (!NextInclude()) break;
    } else if (exclude_start <= include_start &&
               exclude_end < include_end &&
               include_start < exclude_end) {
      // (3) Exclude overlaps with begin of include.
      // Include    #######
      // Exclude  #####
      // Result        ####
      include_start = exclude_end;
      NextExclude();
    } else if (include_start < exclude_start &&
               include_end <= exclude_end &&
               exclude_start < include_end) {
      // (4) Exclude overlaps with end of include.
      // Include    #######
      // Exclude        #####
      // Result     ####
      result.push(new Range(include_start, exclude_start));
      if (!NextInclude()) break;
    } else if (exclude_start > include_start && exclude_end < include_end) {
      // (5) Exclude splits include into two parts.
      // Include    #######
      // Exclude      ##
      // Result     ##  ###
      result.push(new Range(include_start, exclude_start));
      include_start = exclude_end;
      NextExclude();
    } else if (exclude_start <= include_start && exclude_end >= include_end) {
      // (6) Exclude entirely covers include.
      // Include    ######
      // Exclude   #########
      if (!NextInclude()) break;
    } else {
      throw new Error("this should not happen!");
    }
  }

  return result;
}


function GnuplotOutput() {
  xrange_start = (xrange_start_override || xrange_start_override == 0)
                     ? xrange_start_override : xrange_start;
  xrange_end = (xrange_end_override || xrange_end_override == 0)
                   ? xrange_end_override : xrange_end;
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

  // Name Y-axis.
  var ytics = [];
  for (name in TimerEvents) {
    var index = TimerEvents[name].index;
    ytics.push('"' + name + '"' + ' ' + index);
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


CollectData();
GnuplotOutput();
