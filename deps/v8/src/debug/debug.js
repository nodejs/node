// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function (global, utils) {
"use strict";

// ----------------------------------------------------------------------------
// Imports

var FrameMirror = global.FrameMirror;
var GlobalArray = global.Array;
var GlobalRegExp = global.RegExp;
var IsNaN = global.isNaN;
var MakeMirror = global.MakeMirror;
var MathMin = global.Math.min;
var Mirror = global.Mirror;
var ValueMirror = global.ValueMirror;

//----------------------------------------------------------------------------

// Default number of frames to include in the response to backtrace request.
var kDefaultBacktraceLength = 10;

var Debug = {};

// Regular expression to skip "crud" at the beginning of a source line which is
// not really code. Currently the regular expression matches whitespace and
// comments.
var sourceLineBeginningSkip = /^(?:\s*(?:\/\*.*?\*\/)*)*/;

// Debug events which can occour in the V8 JavaScript engine. These originate
// from the API include file debug.h.
Debug.DebugEvent = { Break: 1,
                     Exception: 2,
                     AfterCompile: 3,
                     CompileError: 4,
                     AsyncTaskEvent: 5 };

// Types of exceptions that can be broken upon.
Debug.ExceptionBreak = { Caught : 0,
                         Uncaught: 1 };

// The different types of steps.
Debug.StepAction = { StepOut: 0,
                     StepNext: 1,
                     StepIn: 2 };

// The different types of scripts matching enum ScriptType in objects.h.
Debug.ScriptType = { Native: 0,
                     Extension: 1,
                     Normal: 2,
                     Wasm: 3};

// The different types of script compilations matching enum
// Script::CompilationType in objects.h.
Debug.ScriptCompilationType = { Host: 0,
                                Eval: 1,
                                JSON: 2 };

// The different script break point types.
Debug.ScriptBreakPointType = { ScriptId: 0,
                               ScriptName: 1,
                               ScriptRegExp: 2 };

// The different types of breakpoint position alignments.
// Must match BreakPositionAlignment in debug.h.
Debug.BreakPositionAlignment = {
  Statement: 0,
  BreakPosition: 1
};

function ScriptTypeFlag(type) {
  return (1 << type);
}

// Globals.
var next_response_seq = 0;
var next_break_point_number = 1;
var break_points = [];
var script_break_points = [];
var debugger_flags = {
  breakPointsActive: {
    value: true,
    getValue: function() { return this.value; },
    setValue: function(value) {
      this.value = !!value;
      %SetBreakPointsActive(this.value);
    }
  },
  breakOnCaughtException: {
    getValue: function() { return Debug.isBreakOnException(); },
    setValue: function(value) {
      if (value) {
        Debug.setBreakOnException();
      } else {
        Debug.clearBreakOnException();
      }
    }
  },
  breakOnUncaughtException: {
    getValue: function() { return Debug.isBreakOnUncaughtException(); },
    setValue: function(value) {
      if (value) {
        Debug.setBreakOnUncaughtException();
      } else {
        Debug.clearBreakOnUncaughtException();
      }
    }
  },
};


// Create a new break point object and add it to the list of break points.
function MakeBreakPoint(source_position, opt_script_break_point) {
  var break_point = new BreakPoint(source_position, opt_script_break_point);
  break_points.push(break_point);
  return break_point;
}


// Object representing a break point.
// NOTE: This object does not have a reference to the function having break
// point as this would cause function not to be garbage collected when it is
// not used any more. We do not want break points to keep functions alive.
function BreakPoint(source_position, opt_script_break_point) {
  this.source_position_ = source_position;
  if (opt_script_break_point) {
    this.script_break_point_ = opt_script_break_point;
  } else {
    this.number_ = next_break_point_number++;
  }
  this.active_ = true;
  this.condition_ = null;
}


BreakPoint.prototype.number = function() {
  return this.number_;
};


BreakPoint.prototype.func = function() {
  return this.func_;
};


BreakPoint.prototype.source_position = function() {
  return this.source_position_;
};


BreakPoint.prototype.active = function() {
  if (this.script_break_point()) {
    return this.script_break_point().active();
  }
  return this.active_;
};


BreakPoint.prototype.condition = function() {
  if (this.script_break_point() && this.script_break_point().condition()) {
    return this.script_break_point().condition();
  }
  return this.condition_;
};


BreakPoint.prototype.script_break_point = function() {
  return this.script_break_point_;
};


BreakPoint.prototype.enable = function() {
  this.active_ = true;
};


BreakPoint.prototype.disable = function() {
  this.active_ = false;
};


BreakPoint.prototype.setCondition = function(condition) {
  this.condition_ = condition;
};


BreakPoint.prototype.isTriggered = function(exec_state) {
  // Break point not active - not triggered.
  if (!this.active()) return false;

  // Check for conditional break point.
  if (this.condition()) {
    // If break point has condition try to evaluate it in the top frame.
    try {
      var mirror = exec_state.frame(0).evaluate(this.condition());
      // If no sensible mirror or non true value break point not triggered.
      if (!(mirror instanceof ValueMirror) || !mirror.value_) {
        return false;
      }
    } catch (e) {
      // Exception evaluating condition counts as not triggered.
      return false;
    }
  }

  // Break point triggered.
  return true;
};


// Function called from the runtime when a break point is hit. Returns true if
// the break point is triggered and supposed to break execution.
function IsBreakPointTriggered(break_id, break_point) {
  return break_point.isTriggered(MakeExecutionState(break_id));
}


// Object representing a script break point. The script is referenced by its
// script name or script id and the break point is represented as line and
// column.
function ScriptBreakPoint(type, script_id_or_name, opt_line, opt_column,
                          opt_groupId, opt_position_alignment) {
  this.type_ = type;
  if (type == Debug.ScriptBreakPointType.ScriptId) {
    this.script_id_ = script_id_or_name;
  } else if (type == Debug.ScriptBreakPointType.ScriptName) {
    this.script_name_ = script_id_or_name;
  } else if (type == Debug.ScriptBreakPointType.ScriptRegExp) {
    this.script_regexp_object_ = new GlobalRegExp(script_id_or_name);
  } else {
    throw %make_error(kDebugger, "Unexpected breakpoint type " + type);
  }
  this.line_ = opt_line || 0;
  this.column_ = opt_column;
  this.groupId_ = opt_groupId;
  this.position_alignment_ = IS_UNDEFINED(opt_position_alignment)
      ? Debug.BreakPositionAlignment.Statement : opt_position_alignment;
  this.active_ = true;
  this.condition_ = null;
  this.break_points_ = [];
}


ScriptBreakPoint.prototype.number = function() {
  return this.number_;
};


ScriptBreakPoint.prototype.groupId = function() {
  return this.groupId_;
};


ScriptBreakPoint.prototype.type = function() {
  return this.type_;
};


ScriptBreakPoint.prototype.script_id = function() {
  return this.script_id_;
};


ScriptBreakPoint.prototype.script_name = function() {
  return this.script_name_;
};


ScriptBreakPoint.prototype.script_regexp_object = function() {
  return this.script_regexp_object_;
};


ScriptBreakPoint.prototype.line = function() {
  return this.line_;
};


ScriptBreakPoint.prototype.column = function() {
  return this.column_;
};


ScriptBreakPoint.prototype.actual_locations = function() {
  var locations = [];
  for (var i = 0; i < this.break_points_.length; i++) {
    locations.push(this.break_points_[i].actual_location);
  }
  return locations;
};


ScriptBreakPoint.prototype.update_positions = function(line, column) {
  this.line_ = line;
  this.column_ = column;
};


ScriptBreakPoint.prototype.active = function() {
  return this.active_;
};


ScriptBreakPoint.prototype.condition = function() {
  return this.condition_;
};


ScriptBreakPoint.prototype.enable = function() {
  this.active_ = true;
};


ScriptBreakPoint.prototype.disable = function() {
  this.active_ = false;
};


ScriptBreakPoint.prototype.setCondition = function(condition) {
  this.condition_ = condition;
};


// Check whether a script matches this script break point. Currently this is
// only based on script name.
ScriptBreakPoint.prototype.matchesScript = function(script) {
  if (this.type_ == Debug.ScriptBreakPointType.ScriptId) {
    return this.script_id_ == script.id;
  } else {
    // We might want to account columns here as well.
    if (!(script.line_offset <= this.line_  &&
          this.line_ < script.line_offset + %ScriptLineCount(script))) {
      return false;
    }
    if (this.type_ == Debug.ScriptBreakPointType.ScriptName) {
      return this.script_name_ == script.nameOrSourceURL();
    } else if (this.type_ == Debug.ScriptBreakPointType.ScriptRegExp) {
      return this.script_regexp_object_.test(script.nameOrSourceURL());
    } else {
      throw %make_error(kDebugger, "Unexpected breakpoint type " + this.type_);
    }
  }
};


// Set the script break point in a script.
ScriptBreakPoint.prototype.set = function (script) {
  var column = this.column();
  var line = this.line();
  // If the column is undefined the break is on the line. To help locate the
  // first piece of breakable code on the line try to find the column on the
  // line which contains some source.
  if (IS_UNDEFINED(column)) {
    var source_line = %ScriptSourceLine(script, line || script.line_offset);

    // Allocate array for caching the columns where the actual source starts.
    if (!script.sourceColumnStart_) {
      script.sourceColumnStart_ = new GlobalArray(%ScriptLineCount(script));
    }

    // Fill cache if needed and get column where the actual source starts.
    if (IS_UNDEFINED(script.sourceColumnStart_[line])) {
      script.sourceColumnStart_[line] =
          source_line.match(sourceLineBeginningSkip)[0].length;
    }
    column = script.sourceColumnStart_[line];
  }

  // Convert the line and column into an absolute position within the script.
  var position = Debug.findScriptSourcePosition(script, this.line(), column);

  // If the position is not found in the script (the script might be shorter
  // than it used to be) just ignore it.
  if (IS_NULL(position)) return;

  // Create a break point object and set the break point.
  var break_point = MakeBreakPoint(position, this);
  var actual_position = %SetScriptBreakPoint(script, position,
                                             this.position_alignment_,
                                             break_point);
  if (IS_UNDEFINED(actual_position)) {
    actual_position = position;
  }
  var actual_location = script.locationFromPosition(actual_position, true);
  break_point.actual_location = { line: actual_location.line,
                                  column: actual_location.column,
                                  script_id: script.id };
  this.break_points_.push(break_point);
  return break_point;
};


// Clear all the break points created from this script break point
ScriptBreakPoint.prototype.clear = function () {
  var remaining_break_points = [];
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].script_break_point() &&
        break_points[i].script_break_point() === this) {
      %ClearBreakPoint(break_points[i]);
    } else {
      remaining_break_points.push(break_points[i]);
    }
  }
  break_points = remaining_break_points;
  this.break_points_ = [];
};


Debug.setListener = function(listener, opt_data) {
  if (!IS_FUNCTION(listener) && !IS_UNDEFINED(listener) && !IS_NULL(listener)) {
    throw %make_type_error(kDebuggerType);
  }
  %SetDebugEventListener(listener, opt_data);
};


// Returns a Script object. If the parameter is a function the return value
// is the script in which the function is defined. If the parameter is a string
// the return value is the script for which the script name has that string
// value.  If it is a regexp and there is a unique script whose name matches
// we return that, otherwise undefined.
Debug.findScript = function(func_or_script_name) {
  if (IS_FUNCTION(func_or_script_name)) {
    return %FunctionGetScript(func_or_script_name);
  } else if (%IsRegExp(func_or_script_name)) {
    var scripts = this.scripts();
    var last_result = null;
    var result_count = 0;
    for (var i in scripts) {
      var script = scripts[i];
      if (func_or_script_name.test(script.name)) {
        last_result = script;
        result_count++;
      }
    }
    // Return the unique script matching the regexp.  If there are more
    // than one we don't return a value since there is no good way to
    // decide which one to return.  Returning a "random" one, say the
    // first, would introduce nondeterminism (or something close to it)
    // because the order is the heap iteration order.
    if (result_count == 1) {
      return last_result;
    } else {
      return UNDEFINED;
    }
  } else {
    return %GetScript(func_or_script_name);
  }
};

// Returns the script source. If the parameter is a function the return value
// is the script source for the script in which the function is defined. If the
// parameter is a string the return value is the script for which the script
// name has that string value.
Debug.scriptSource = function(func_or_script_name) {
  return this.findScript(func_or_script_name).source;
};


Debug.source = function(f) {
  if (!IS_FUNCTION(f)) throw %make_type_error(kDebuggerType);
  return %FunctionGetSourceCode(f);
};


Debug.sourcePosition = function(f) {
  if (!IS_FUNCTION(f)) throw %make_type_error(kDebuggerType);
  return %FunctionGetScriptSourcePosition(f);
};


Debug.findFunctionSourceLocation = function(func, opt_line, opt_column) {
  var script = %FunctionGetScript(func);
  var script_offset = %FunctionGetScriptSourcePosition(func);
  return %ScriptLocationFromLine(script, opt_line, opt_column, script_offset);
};


// Returns the character position in a script based on a line number and an
// optional position within that line.
Debug.findScriptSourcePosition = function(script, opt_line, opt_column) {
  var location = %ScriptLocationFromLine(script, opt_line, opt_column, 0);
  return location ? location.position : null;
};


Debug.findBreakPoint = function(break_point_number, remove) {
  var break_point;
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].number() == break_point_number) {
      break_point = break_points[i];
      // Remove the break point from the list if requested.
      if (remove) {
        break_points.splice(i, 1);
      }
      break;
    }
  }
  if (break_point) {
    return break_point;
  } else {
    return this.findScriptBreakPoint(break_point_number, remove);
  }
};

Debug.findBreakPointActualLocations = function(break_point_number) {
  for (var i = 0; i < script_break_points.length; i++) {
    if (script_break_points[i].number() == break_point_number) {
      return script_break_points[i].actual_locations();
    }
  }
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].number() == break_point_number) {
      return [break_points[i].actual_location];
    }
  }
  return [];
};

Debug.setBreakPoint = function(func, opt_line, opt_column, opt_condition) {
  if (!IS_FUNCTION(func)) throw %make_type_error(kDebuggerType);
  // Break points in API functions are not supported.
  if (%FunctionIsAPIFunction(func)) {
    throw %make_error(kDebugger, 'Cannot set break point in native code.');
  }
  // Find source position.
  var source_position =
      this.findFunctionSourceLocation(func, opt_line, opt_column).position;
  // Find the script for the function.
  var script = %FunctionGetScript(func);
  // Break in builtin JavaScript code is not supported.
  if (script.type == Debug.ScriptType.Native) {
    throw %make_error(kDebugger, 'Cannot set break point in native code.');
  }
  // If the script for the function has a name convert this to a script break
  // point.
  if (script && script.id) {
    // Find line and column for the position in the script and set a script
    // break point from that.
    var location = script.locationFromPosition(source_position, false);
    return this.setScriptBreakPointById(script.id,
                                        location.line, location.column,
                                        opt_condition);
  } else {
    // Set a break point directly on the function.
    var break_point = MakeBreakPoint(source_position);
    var actual_position =
        %SetFunctionBreakPoint(func, source_position, break_point);
    var actual_location = script.locationFromPosition(actual_position, true);
    break_point.actual_location = { line: actual_location.line,
                                    column: actual_location.column,
                                    script_id: script.id };
    break_point.setCondition(opt_condition);
    return break_point.number();
  }
};


Debug.setBreakPointByScriptIdAndPosition = function(script_id, position,
                                                    condition, enabled,
                                                    opt_position_alignment)
{
  var break_point = MakeBreakPoint(position);
  break_point.setCondition(condition);
  if (!enabled) {
    break_point.disable();
  }
  var script = scriptById(script_id);
  if (script) {
    var position_alignment = IS_UNDEFINED(opt_position_alignment)
        ? Debug.BreakPositionAlignment.Statement : opt_position_alignment;
    break_point.actual_position = %SetScriptBreakPoint(script, position,
        position_alignment, break_point);
  }
  return break_point;
};


Debug.enableBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, false);
  // Only enable if the breakpoint hasn't been deleted:
  if (break_point) {
    break_point.enable();
  }
};


Debug.disableBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, false);
  // Only enable if the breakpoint hasn't been deleted:
  if (break_point) {
    break_point.disable();
  }
};


Debug.changeBreakPointCondition = function(break_point_number, condition) {
  var break_point = this.findBreakPoint(break_point_number, false);
  break_point.setCondition(condition);
};


Debug.clearBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, true);
  if (break_point) {
    return %ClearBreakPoint(break_point);
  } else {
    break_point = this.findScriptBreakPoint(break_point_number, true);
    if (!break_point) throw %make_error(kDebugger, 'Invalid breakpoint');
  }
};


Debug.clearAllBreakPoints = function() {
  for (var i = 0; i < break_points.length; i++) {
    var break_point = break_points[i];
    %ClearBreakPoint(break_point);
  }
  break_points = [];
};


Debug.disableAllBreakPoints = function() {
  // Disable all user defined breakpoints:
  for (var i = 1; i < next_break_point_number; i++) {
    Debug.disableBreakPoint(i);
  }
  // Disable all exception breakpoints:
  %ChangeBreakOnException(Debug.ExceptionBreak.Caught, false);
  %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, false);
};


Debug.findScriptBreakPoint = function(break_point_number, remove) {
  var script_break_point;
  for (var i = 0; i < script_break_points.length; i++) {
    if (script_break_points[i].number() == break_point_number) {
      script_break_point = script_break_points[i];
      // Remove the break point from the list if requested.
      if (remove) {
        script_break_point.clear();
        script_break_points.splice(i,1);
      }
      break;
    }
  }
  return script_break_point;
};


// Sets a breakpoint in a script identified through id or name at the
// specified source line and column within that line.
Debug.setScriptBreakPoint = function(type, script_id_or_name,
                                     opt_line, opt_column, opt_condition,
                                     opt_groupId, opt_position_alignment) {
  // Create script break point object.
  var script_break_point =
      new ScriptBreakPoint(type, script_id_or_name, opt_line, opt_column,
                           opt_groupId, opt_position_alignment);

  // Assign number to the new script break point and add it.
  script_break_point.number_ = next_break_point_number++;
  script_break_point.setCondition(opt_condition);
  script_break_points.push(script_break_point);

  // Run through all scripts to see if this script break point matches any
  // loaded scripts.
  var scripts = this.scripts();
  for (var i = 0; i < scripts.length; i++) {
    if (script_break_point.matchesScript(scripts[i])) {
      script_break_point.set(scripts[i]);
    }
  }

  return script_break_point.number();
};


Debug.setScriptBreakPointById = function(script_id,
                                         opt_line, opt_column,
                                         opt_condition, opt_groupId,
                                         opt_position_alignment) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptId,
                                  script_id, opt_line, opt_column,
                                  opt_condition, opt_groupId,
                                  opt_position_alignment);
};


Debug.setScriptBreakPointByName = function(script_name,
                                           opt_line, opt_column,
                                           opt_condition, opt_groupId) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptName,
                                  script_name, opt_line, opt_column,
                                  opt_condition, opt_groupId);
};


Debug.setScriptBreakPointByRegExp = function(script_regexp,
                                             opt_line, opt_column,
                                             opt_condition, opt_groupId) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptRegExp,
                                  script_regexp, opt_line, opt_column,
                                  opt_condition, opt_groupId);
};


Debug.enableScriptBreakPoint = function(break_point_number) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.enable();
};


Debug.disableScriptBreakPoint = function(break_point_number) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.disable();
};


Debug.changeScriptBreakPointCondition = function(
    break_point_number, condition) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.setCondition(condition);
};


Debug.scriptBreakPoints = function() {
  return script_break_points;
};


Debug.clearStepping = function() {
  %ClearStepping();
};

Debug.setBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Caught, true);
};

Debug.clearBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Caught, false);
};

Debug.isBreakOnException = function() {
  return !!%IsBreakOnException(Debug.ExceptionBreak.Caught);
};

Debug.setBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, true);
};

Debug.clearBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, false);
};

Debug.isBreakOnUncaughtException = function() {
  return !!%IsBreakOnException(Debug.ExceptionBreak.Uncaught);
};

Debug.showBreakPoints = function(f, full, opt_position_alignment) {
  if (!IS_FUNCTION(f)) throw %make_error(kDebuggerType);
  var source = full ? this.scriptSource(f) : this.source(f);
  var offset = full ? 0 : this.sourcePosition(f);
  var position_alignment = IS_UNDEFINED(opt_position_alignment)
      ? Debug.BreakPositionAlignment.Statement : opt_position_alignment;
  var locations = %GetBreakLocations(f, position_alignment);
  if (!locations) return source;
  locations.sort(function(x, y) { return x - y; });
  var result = "";
  var prev_pos = 0;
  var pos;
  for (var i = 0; i < locations.length; i++) {
    pos = locations[i] - offset;
    result += source.slice(prev_pos, pos);
    result += "[B" + i + "]";
    prev_pos = pos;
  }
  pos = source.length;
  result += source.substring(prev_pos, pos);
  return result;
};


// Get all the scripts currently loaded. Locating all the scripts is based on
// scanning the heap.
Debug.scripts = function() {
  // Collect all scripts in the heap.
  return %DebugGetLoadedScripts();
};


// Get a specific script currently loaded. This is based on scanning the heap.
// TODO(clemensh): Create a runtime function for this.
function scriptById(scriptId) {
  var scripts = Debug.scripts();
  for (var script of scripts) {
    if (script.id == scriptId) return script;
  }
  return UNDEFINED;
};


Debug.debuggerFlags = function() {
  return debugger_flags;
};

Debug.MakeMirror = MakeMirror;

function MakeExecutionState(break_id) {
  return new ExecutionState(break_id);
}

function ExecutionState(break_id) {
  this.break_id = break_id;
  this.selected_frame = 0;
}

ExecutionState.prototype.prepareStep = function(action) {
  if (action === Debug.StepAction.StepIn ||
      action === Debug.StepAction.StepOut ||
      action === Debug.StepAction.StepNext) {
    return %PrepareStep(this.break_id, action);
  }
  throw %make_type_error(kDebuggerType);
};

ExecutionState.prototype.evaluateGlobal = function(source) {
  return MakeMirror(%DebugEvaluateGlobal(this.break_id, source));
};

ExecutionState.prototype.frameCount = function() {
  return %GetFrameCount(this.break_id);
};

ExecutionState.prototype.frame = function(opt_index) {
  // If no index supplied return the selected frame.
  if (opt_index == null) opt_index = this.selected_frame;
  if (opt_index < 0 || opt_index >= this.frameCount()) {
    throw %make_type_error(kDebuggerFrame);
  }
  return new FrameMirror(this.break_id, opt_index);
};

ExecutionState.prototype.setSelectedFrame = function(index) {
  var i = TO_NUMBER(index);
  if (i < 0 || i >= this.frameCount()) {
    throw %make_type_error(kDebuggerFrame);
  }
  this.selected_frame = i;
};

ExecutionState.prototype.selectedFrame = function() {
  return this.selected_frame;
};

function MakeBreakEvent(break_id, break_points_hit) {
  return new BreakEvent(break_id, break_points_hit);
}


function BreakEvent(break_id, break_points_hit) {
  this.frame_ = new FrameMirror(break_id, 0);
  this.break_points_hit_ = break_points_hit;
}


BreakEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Break;
};


BreakEvent.prototype.func = function() {
  return this.frame_.func();
};


BreakEvent.prototype.sourceLine = function() {
  return this.frame_.sourceLine();
};


BreakEvent.prototype.sourceColumn = function() {
  return this.frame_.sourceColumn();
};


BreakEvent.prototype.sourceLineText = function() {
  return this.frame_.sourceLineText();
};


BreakEvent.prototype.breakPointsHit = function() {
  return this.break_points_hit_;
};


function MakeExceptionEvent(break_id, exception, uncaught, promise) {
  return new ExceptionEvent(break_id, exception, uncaught, promise);
}


function ExceptionEvent(break_id, exception, uncaught, promise) {
  this.exec_state_ = new ExecutionState(break_id);
  this.exception_ = exception;
  this.uncaught_ = uncaught;
  this.promise_ = promise;
}


ExceptionEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Exception;
};


ExceptionEvent.prototype.exception = function() {
  return this.exception_;
};


ExceptionEvent.prototype.uncaught = function() {
  return this.uncaught_;
};


ExceptionEvent.prototype.promise = function() {
  return this.promise_;
};


ExceptionEvent.prototype.func = function() {
  return this.exec_state_.frame(0).func();
};


ExceptionEvent.prototype.sourceLine = function() {
  return this.exec_state_.frame(0).sourceLine();
};


ExceptionEvent.prototype.sourceColumn = function() {
  return this.exec_state_.frame(0).sourceColumn();
};


ExceptionEvent.prototype.sourceLineText = function() {
  return this.exec_state_.frame(0).sourceLineText();
};


function MakeCompileEvent(script, type) {
  return new CompileEvent(script, type);
}


function CompileEvent(script, type) {
  this.script_ = MakeMirror(script);
  this.type_ = type;
}


CompileEvent.prototype.eventType = function() {
  return this.type_;
};


CompileEvent.prototype.script = function() {
  return this.script_;
};


function MakeScriptObject_(script, include_source) {
  var o = { id: script.id(),
            name: script.name(),
            lineOffset: script.lineOffset(),
            columnOffset: script.columnOffset(),
            lineCount: script.lineCount(),
          };
  if (!IS_UNDEFINED(script.data())) {
    o.data = script.data();
  }
  if (include_source) {
    o.source = script.source();
  }
  return o;
}


function MakeAsyncTaskEvent(type, id) {
  return new AsyncTaskEvent(type, id);
}


function AsyncTaskEvent(type, id) {
  this.type_ = type;
  this.id_ = id;
}


AsyncTaskEvent.prototype.type = function() {
  return this.type_;
}


AsyncTaskEvent.prototype.id = function() {
  return this.id_;
}

// -------------------------------------------------------------------
// Exports

utils.InstallConstants(global, [
  "Debug", Debug,
  "BreakEvent", BreakEvent,
  "CompileEvent", CompileEvent,
  "BreakPoint", BreakPoint,
]);

// Functions needed by the debugger runtime.
utils.InstallFunctions(utils, DONT_ENUM, [
  "MakeExecutionState", MakeExecutionState,
  "MakeExceptionEvent", MakeExceptionEvent,
  "MakeBreakEvent", MakeBreakEvent,
  "MakeCompileEvent", MakeCompileEvent,
  "MakeAsyncTaskEvent", MakeAsyncTaskEvent,
  "IsBreakPointTriggered", IsBreakPointTriggered,
]);

})
