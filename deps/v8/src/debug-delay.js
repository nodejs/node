// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

// jsminify this file, js2c: jsmin

// Default number of frames to include in the response to backtrace request.
const kDefaultBacktraceLength = 10;

const Debug = {};

// Regular expression to skip "crud" at the beginning of a source line which is
// not really code. Currently the regular expression matches whitespace and
// comments.
const sourceLineBeginningSkip = /^(?:[ \v\h]*(?:\/\*.*?\*\/)*)*/;

// Debug events which can occour in the V8 JavaScript engine. These originate
// from the API include file debug.h.
Debug.DebugEvent = { Break: 1,
                     Exception: 2,
                     NewFunction: 3,
                     BeforeCompile: 4,
                     AfterCompile: 5,
                     ScriptCollected: 6 };

// Types of exceptions that can be broken upon.
Debug.ExceptionBreak = { All : 0,
                         Uncaught: 1 };

// The different types of steps.
Debug.StepAction = { StepOut: 0,
                     StepNext: 1,
                     StepIn: 2,
                     StepMin: 3,
                     StepInMin: 4 };

// The different types of scripts matching enum ScriptType in objects.h.
Debug.ScriptType = { Native: 0,
                     Extension: 1,
                     Normal: 2 };

// The different types of script compilations matching enum
// Script::CompilationType in objects.h.
Debug.ScriptCompilationType = { Host: 0,
                                Eval: 1,
                                JSON: 2 };

// The different script break point types.
Debug.ScriptBreakPointType = { ScriptId: 0,
                               ScriptName: 1 };

function ScriptTypeFlag(type) {
  return (1 << type);
}

// Globals.
var next_response_seq = 0;
var next_break_point_number = 1;
var break_points = [];
var script_break_points = [];


// Create a new break point object and add it to the list of break points.
function MakeBreakPoint(source_position, opt_line, opt_column, opt_script_break_point) {
  var break_point = new BreakPoint(source_position, opt_line, opt_column, opt_script_break_point);
  break_points.push(break_point);
  return break_point;
}


// Object representing a break point.
// NOTE: This object does not have a reference to the function having break
// point as this would cause function not to be garbage collected when it is
// not used any more. We do not want break points to keep functions alive.
function BreakPoint(source_position, opt_line, opt_column, opt_script_break_point) {
  this.source_position_ = source_position;
  this.source_line_ = opt_line;
  this.source_column_ = opt_column;
  if (opt_script_break_point) {
    this.script_break_point_ = opt_script_break_point;
  } else {
    this.number_ = next_break_point_number++;
  }
  this.hit_count_ = 0;
  this.active_ = true;
  this.condition_ = null;
  this.ignoreCount_ = 0;
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


BreakPoint.prototype.hit_count = function() {
  return this.hit_count_;
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


BreakPoint.prototype.ignoreCount = function() {
  return this.ignoreCount_;
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


BreakPoint.prototype.setIgnoreCount = function(ignoreCount) {
  this.ignoreCount_ = ignoreCount;
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
      if (!(mirror instanceof ValueMirror) || !%ToBoolean(mirror.value_)) {
        return false;
      }
    } catch (e) {
      // Exception evaluating condition counts as not triggered.
      return false;
    }
  }

  // Update the hit count.
  this.hit_count_++;
  if (this.script_break_point_) {
    this.script_break_point_.hit_count_++;
  }

  // If the break point has an ignore count it is not triggered.
  if (this.ignoreCount_ > 0) {
    this.ignoreCount_--;
    return false;
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
function ScriptBreakPoint(type, script_id_or_name, opt_line, opt_column) {
  this.type_ = type;
  if (type == Debug.ScriptBreakPointType.ScriptId) {
    this.script_id_ = script_id_or_name;
  } else {  // type == Debug.ScriptBreakPointType.ScriptName
    this.script_name_ = script_id_or_name;
  }
  this.line_ = opt_line || 0;
  this.column_ = opt_column;
  this.hit_count_ = 0;
  this.active_ = true;
  this.condition_ = null;
  this.ignoreCount_ = 0;
}


ScriptBreakPoint.prototype.number = function() {
  return this.number_;
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


ScriptBreakPoint.prototype.line = function() {
  return this.line_;
};


ScriptBreakPoint.prototype.column = function() {
  return this.column_;
};


ScriptBreakPoint.prototype.hit_count = function() {
  return this.hit_count_;
};


ScriptBreakPoint.prototype.active = function() {
  return this.active_;
};


ScriptBreakPoint.prototype.condition = function() {
  return this.condition_;
};


ScriptBreakPoint.prototype.ignoreCount = function() {
  return this.ignoreCount_;
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


ScriptBreakPoint.prototype.setIgnoreCount = function(ignoreCount) {
  this.ignoreCount_ = ignoreCount;

  // Set ignore count on all break points created from this script break point.
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].script_break_point() === this) {
      break_points[i].setIgnoreCount(ignoreCount);
    }
  }
};


// Check whether a script matches this script break point. Currently this is
// only based on script name.
ScriptBreakPoint.prototype.matchesScript = function(script) {
  if (this.type_ == Debug.ScriptBreakPointType.ScriptId) {
    return this.script_id_ == script.id;
  } else {  // this.type_ == Debug.ScriptBreakPointType.ScriptName
    return this.script_name_ == script.name &&
           script.line_offset <= this.line_  &&
           this.line_ < script.line_offset + script.lineCount();
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
    var source_line = script.sourceLine(this.line());

    // Allocate array for caching the columns where the actual source starts.
    if (!script.sourceColumnStart_) {
      script.sourceColumnStart_ = new Array(script.lineCount());
    }
    
    // Fill cache if needed and get column where the actual source starts.
    if (IS_UNDEFINED(script.sourceColumnStart_[line])) {
      script.sourceColumnStart_[line] =
          source_line.match(sourceLineBeginningSkip)[0].length;
    }
    column = script.sourceColumnStart_[line];
  }

  // Convert the line and column into an absolute position within the script.
  var pos = Debug.findScriptSourcePosition(script, this.line(), column);
  
  // If the position is not found in the script (the script might be shorter
  // than it used to be) just ignore it.
  if (pos === null) return;
  
  // Create a break point object and set the break point.
  break_point = MakeBreakPoint(pos, this.line(), this.column(), this);
  break_point.setIgnoreCount(this.ignoreCount());
  %SetScriptBreakPoint(script, pos, break_point);

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
};


// Function called from runtime when a new script is compiled to set any script
// break points set in this script.
function UpdateScriptBreakPoints(script) {
  for (var i = 0; i < script_break_points.length; i++) {
    if (script_break_points[i].type() == Debug.ScriptBreakPointType.ScriptName &&
        script_break_points[i].matchesScript(script)) {
      script_break_points[i].set(script);
    }
  }
}


Debug.setListener = function(listener, opt_data) {
  if (!IS_FUNCTION(listener) && !IS_UNDEFINED(listener) && !IS_NULL(listener)) {
    throw new Error('Parameters have wrong types.');
  }
  %SetDebugEventListener(listener, opt_data);
};


Debug.breakExecution = function(f) {
  %Break();
};

Debug.breakLocations = function(f) {
  if (!IS_FUNCTION(f)) throw new Error('Parameters have wrong types.');
  return %GetBreakLocations(f);
};

// Returns a Script object. If the parameter is a function the return value
// is the script in which the function is defined. If the parameter is a string
// the return value is the script for which the script name has that string
// value.  If it is a regexp and there is a unique script whose name matches
// we return that, otherwise undefined.
Debug.findScript = function(func_or_script_name) {
  if (IS_FUNCTION(func_or_script_name)) {
    return %FunctionGetScript(func_or_script_name);
  } else if (IS_REGEXP(func_or_script_name)) {
    var scripts = Debug.scripts();
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
      return undefined;
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
  if (!IS_FUNCTION(f)) throw new Error('Parameters have wrong types.');
  return %FunctionGetSourceCode(f);
};

Debug.assembler = function(f) {
  if (!IS_FUNCTION(f)) throw new Error('Parameters have wrong types.');
  return %FunctionGetAssemblerCode(f);
};

Debug.sourcePosition = function(f) {
  if (!IS_FUNCTION(f)) throw new Error('Parameters have wrong types.');
  return %FunctionGetScriptSourcePosition(f);
};


Debug.findFunctionSourceLocation = function(func, opt_line, opt_column) {
  var script = %FunctionGetScript(func);
  var script_offset = %FunctionGetScriptSourcePosition(func);
  return script.locationFromLine(opt_line, opt_column, script_offset);
}


// Returns the character position in a script based on a line number and an
// optional position within that line.
Debug.findScriptSourcePosition = function(script, opt_line, opt_column) {
  var location = script.locationFromLine(opt_line, opt_column);  
  return location ? location.position : null;
}


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


Debug.setBreakPoint = function(func, opt_line, opt_column, opt_condition) {
  if (!IS_FUNCTION(func)) throw new Error('Parameters have wrong types.');
  // Break points in API functions are not supported.
  if (%FunctionIsAPIFunction(func)) {
    throw new Error('Cannot set break point in native code.');
  }
  // Find source position relative to start of the function
  var break_position =
      this.findFunctionSourceLocation(func, opt_line, opt_column).position;
  var source_position = break_position - this.sourcePosition(func);
  // Find the script for the function.
  var script = %FunctionGetScript(func);
  // Break in builtin JavaScript code is not supported.
  if (script.type == Debug.ScriptType.Native) {
    throw new Error('Cannot set break point in native code.');
  }
  // If the script for the function has a name convert this to a script break
  // point.
  if (script && script.id) {
    // Adjust the source position to be script relative.
    source_position += %FunctionGetScriptSourcePosition(func);
    // Find line and column for the position in the script and set a script
    // break point from that.
    var location = script.locationFromPosition(source_position, false);
    return this.setScriptBreakPointById(script.id,
                                        location.line, location.column,
                                        opt_condition);
  } else {
    // Set a break point directly on the function.
    var break_point = MakeBreakPoint(source_position, opt_line, opt_column);
    %SetFunctionBreakPoint(func, source_position, break_point);
    break_point.setCondition(opt_condition);
    return break_point.number();
  }
};


Debug.enableBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, false);
  break_point.enable();
};


Debug.disableBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, false);
  break_point.disable();
};


Debug.changeBreakPointCondition = function(break_point_number, condition) {
  var break_point = this.findBreakPoint(break_point_number, false);
  break_point.setCondition(condition);
};


Debug.changeBreakPointIgnoreCount = function(break_point_number, ignoreCount) {
  if (ignoreCount < 0) {
    throw new Error('Invalid argument');
  }
  var break_point = this.findBreakPoint(break_point_number, false);
  break_point.setIgnoreCount(ignoreCount);
};


Debug.clearBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, true);
  if (break_point) {
    return %ClearBreakPoint(break_point);
  } else {
    break_point = this.findScriptBreakPoint(break_point_number, true);
    if (!break_point) {
      throw new Error('Invalid breakpoint');
    }
  }
};


Debug.clearAllBreakPoints = function() {
  for (var i = 0; i < break_points.length; i++) {
    break_point = break_points[i];
    %ClearBreakPoint(break_point);
  }
  break_points = [];
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
}


// Sets a breakpoint in a script identified through id or name at the
// specified source line and column within that line.
Debug.setScriptBreakPoint = function(type, script_id_or_name,
                                     opt_line, opt_column, opt_condition) {
  // Create script break point object.
  var script_break_point =
      new ScriptBreakPoint(type, script_id_or_name, opt_line, opt_column);

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
}


Debug.setScriptBreakPointById = function(script_id,
                                         opt_line, opt_column,
                                         opt_condition) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptId,
                                  script_id, opt_line, opt_column,
                                  opt_condition)
}


Debug.setScriptBreakPointByName = function(script_name,
                                           opt_line, opt_column,
                                           opt_condition) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptName,
                                  script_name, opt_line, opt_column,
                                  opt_condition)
}


Debug.enableScriptBreakPoint = function(break_point_number) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.enable();
};


Debug.disableScriptBreakPoint = function(break_point_number) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.disable();
};


Debug.changeScriptBreakPointCondition = function(break_point_number, condition) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.setCondition(condition);
};


Debug.changeScriptBreakPointIgnoreCount = function(break_point_number, ignoreCount) {
  if (ignoreCount < 0) {
    throw new Error('Invalid argument');
  }
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.setIgnoreCount(ignoreCount);
};


Debug.scriptBreakPoints = function() {
  return script_break_points;
}


Debug.clearStepping = function() {
  %ClearStepping();
}

Debug.setBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.All, true);
};

Debug.clearBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.All, false);
};

Debug.setBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, true);
};

Debug.clearBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, false);
};

Debug.showBreakPoints = function(f, full) {
  if (!IS_FUNCTION(f)) throw new Error('Parameters have wrong types.');
  var source = full ? this.scriptSource(f) : this.source(f);
  var offset = full ? this.sourcePosition(f) : 0;
  var locations = this.breakLocations(f);
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
}

function MakeExecutionState(break_id) {
  return new ExecutionState(break_id);
}

function ExecutionState(break_id) {
  this.break_id = break_id;
  this.selected_frame = 0;
}

ExecutionState.prototype.prepareStep = function(opt_action, opt_count) {
  var action = Debug.StepAction.StepIn;
  if (!IS_UNDEFINED(opt_action)) action = %ToNumber(opt_action);
  var count = opt_count ? %ToNumber(opt_count) : 1;

  return %PrepareStep(this.break_id, action, count);
}

ExecutionState.prototype.evaluateGlobal = function(source, disable_break) {
  return MakeMirror(
      %DebugEvaluateGlobal(this.break_id, source, Boolean(disable_break)));
};

ExecutionState.prototype.frameCount = function() {
  return %GetFrameCount(this.break_id);
};

ExecutionState.prototype.threadCount = function() {
  return %GetThreadCount(this.break_id);
};

ExecutionState.prototype.frame = function(opt_index) {
  // If no index supplied return the selected frame.
  if (opt_index == null) opt_index = this.selected_frame;
  return new FrameMirror(this.break_id, opt_index);
};

ExecutionState.prototype.cframesValue = function(opt_from_index, opt_to_index) {
  return %GetCFrames(this.break_id);
};

ExecutionState.prototype.setSelectedFrame = function(index) {
  var i = %ToNumber(index);
  if (i < 0 || i >= this.frameCount()) throw new Error('Illegal frame index.');
  this.selected_frame = i;
};

ExecutionState.prototype.selectedFrame = function() {
  return this.selected_frame;
};

ExecutionState.prototype.debugCommandProcessor = function(protocol) {
  return new DebugCommandProcessor(this, protocol);
};


function MakeBreakEvent(exec_state, break_points_hit) {
  return new BreakEvent(exec_state, break_points_hit);
}


function BreakEvent(exec_state, break_points_hit) {
  this.exec_state_ = exec_state;
  this.break_points_hit_ = break_points_hit;
}


BreakEvent.prototype.executionState = function() {
  return this.exec_state_;
};


BreakEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Break;
};


BreakEvent.prototype.func = function() {
  return this.exec_state_.frame(0).func();
};


BreakEvent.prototype.sourceLine = function() {
  return this.exec_state_.frame(0).sourceLine();
};


BreakEvent.prototype.sourceColumn = function() {
  return this.exec_state_.frame(0).sourceColumn();
};


BreakEvent.prototype.sourceLineText = function() {
  return this.exec_state_.frame(0).sourceLineText();
};


BreakEvent.prototype.breakPointsHit = function() {
  return this.break_points_hit_;
};


BreakEvent.prototype.toJSONProtocol = function() {
  var o = { seq: next_response_seq++,
            type: "event",
            event: "break",
            body: { invocationText: this.exec_state_.frame(0).invocationText(),
                  }
          };

  // Add script related information to the event if available.
  var script = this.func().script();
  if (script) {
    o.body.sourceLine = this.sourceLine(),
    o.body.sourceColumn = this.sourceColumn(),
    o.body.sourceLineText = this.sourceLineText(),
    o.body.script = MakeScriptObject_(script, false);
  }

  // Add an Array of break points hit if any.
  if (this.breakPointsHit()) {
    o.body.breakpoints = [];
    for (var i = 0; i < this.breakPointsHit().length; i++) {
      // Find the break point number. For break points originating from a
      // script break point supply the script break point number.
      var breakpoint = this.breakPointsHit()[i];
      var script_break_point = breakpoint.script_break_point();
      var number;
      if (script_break_point) {
        number = script_break_point.number();
      } else {
        number = breakpoint.number();
      }
      o.body.breakpoints.push(number);
    }
  }
  return JSON.stringify(ObjectToProtocolObject_(o));
};


function MakeExceptionEvent(exec_state, exception, uncaught) {
  return new ExceptionEvent(exec_state, exception, uncaught);
}


function ExceptionEvent(exec_state, exception, uncaught) {
  this.exec_state_ = exec_state;
  this.exception_ = exception;
  this.uncaught_ = uncaught;
}


ExceptionEvent.prototype.executionState = function() {
  return this.exec_state_;
};


ExceptionEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Exception;
};


ExceptionEvent.prototype.exception = function() {
  return this.exception_;
}


ExceptionEvent.prototype.uncaught = function() {
  return this.uncaught_;
}


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


ExceptionEvent.prototype.toJSONProtocol = function() {
  var o = new ProtocolMessage();
  o.event = "exception";
  o.body = { uncaught: this.uncaught_,
             exception: MakeMirror(this.exception_)
           };
           
  // Exceptions might happen whithout any JavaScript frames.
  if (this.exec_state_.frameCount() > 0) {
    o.body.sourceLine = this.sourceLine();
    o.body.sourceColumn = this.sourceColumn();
    o.body.sourceLineText = this.sourceLineText();

    // Add script information to the event if available.
    var script = this.func().script();
    if (script) {
      o.body.script = MakeScriptObject_(script, false);
    }
  } else {
    o.body.sourceLine = -1;
  }

  return o.toJSONProtocol();
};


function MakeCompileEvent(exec_state, script, before) {
  return new CompileEvent(exec_state, script, before);
}


function CompileEvent(exec_state, script, before) {
  this.exec_state_ = exec_state;
  this.script_ = MakeMirror(script);
  this.before_ = before;
}


CompileEvent.prototype.executionState = function() {
  return this.exec_state_;
};


CompileEvent.prototype.eventType = function() {
  if (this.before_) {
    return Debug.DebugEvent.BeforeCompile;
  } else {
    return Debug.DebugEvent.AfterCompile;
  }
};


CompileEvent.prototype.script = function() {
  return this.script_;
};


CompileEvent.prototype.toJSONProtocol = function() {
  var o = new ProtocolMessage();
  o.running = true;
  if (this.before_) {
    o.event = "beforeCompile";
  } else {
    o.event = "afterCompile";
  }
  o.body = {};
  o.body.script = this.script_;
  o.setOption('includeSource', true);

  return o.toJSONProtocol();
}


function MakeNewFunctionEvent(func) {
  return new NewFunctionEvent(func);
}


function NewFunctionEvent(func) {
  this.func = func;
}


NewFunctionEvent.prototype.eventType = function() {
  return Debug.DebugEvent.NewFunction;
};


NewFunctionEvent.prototype.name = function() {
  return this.func.name;
};


NewFunctionEvent.prototype.setBreakPoint = function(p) {
  Debug.setBreakPoint(this.func, p || 0);
};


function MakeScriptCollectedEvent(exec_state, id) {
  return new ScriptCollectedEvent(exec_state, id);
}


function ScriptCollectedEvent(exec_state, id) {
  this.exec_state_ = exec_state;
  this.id_ = id;
}


ScriptCollectedEvent.prototype.id = function() {
  return this.id_;
};


ScriptCollectedEvent.prototype.executionState = function() {
  return this.exec_state_;
};


ScriptCollectedEvent.prototype.toJSONProtocol = function() {
  var o = new ProtocolMessage();
  o.running = true;
  o.event = "scriptCollected";
  o.body = {};
  o.body.script = { id: this.id() };
  return o.toJSONProtocol();
}


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
};


function DebugCommandProcessor(exec_state) {
  this.exec_state_ = exec_state;
  this.running_ = false;
};


DebugCommandProcessor.prototype.processDebugRequest = function (request) {
  return this.processDebugJSONRequest(request);
}


function ProtocolMessage(request) {
  // Update sequence number.
  this.seq = next_response_seq++;
  
  if (request) {
    // If message is based on a request this is a response. Fill the initial
    // response from the request.
    this.type = 'response';
    this.request_seq = request.seq;
    this.command = request.command;
  } else {
    // If message is not based on a request it is a dabugger generated event.
    this.type = 'event';
  }
  this.success = true;
  this.running = false;
}


ProtocolMessage.prototype.setOption = function(name, value) {
  if (!this.options_) {
    this.options_ = {};
  }
  this.options_[name] = value;
}


ProtocolMessage.prototype.failed = function(message) {
  this.success = false;
  this.message = message;
}


ProtocolMessage.prototype.toJSONProtocol = function() {
  // Encode the protocol header.
  var json = {};
  json.seq= this.seq;
  if (this.request_seq) {
    json.request_seq = this.request_seq;
  }
  json.type = this.type;
  if (this.event) {
    json.event = this.event;
  }
  if (this.command) {
    json.command = this.command;
  }
  if (this.success) {
    json.success = this.success;
  } else {
    json.success = false;
  }
  if (this.body) {
    // Encode the body part.
    var bodyJson;
    var serializer = MakeMirrorSerializer(true, this.options_);
    if (this.body instanceof Mirror) {
      bodyJson = serializer.serializeValue(this.body);
    } else if (this.body instanceof Array) {
      bodyJson = [];
      for (var i = 0; i < this.body.length; i++) {
        if (this.body[i] instanceof Mirror) {
          bodyJson.push(serializer.serializeValue(this.body[i]));
        } else {
          bodyJson.push(ObjectToProtocolObject_(this.body[i], serializer));
        }
      }
    } else {
      bodyJson = ObjectToProtocolObject_(this.body, serializer);
    }
    json.body = bodyJson;
    json.refs = serializer.serializeReferencedObjects();
  }
  if (this.message) {
    json.message = this.message;
  }
  if (this.running) {
    json.running = true;
  } else {
    json.running = false;
  }
  return JSON.stringify(json);
}


DebugCommandProcessor.prototype.createResponse = function(request) {
  return new ProtocolMessage(request);
};


DebugCommandProcessor.prototype.processDebugJSONRequest = function(json_request) {
  var request;  // Current request.
  var response;  // Generated response.
  try {
    try {
      // Convert the JSON string to an object.
      request = %CompileString('(' + json_request + ')', false)();

      // Create an initial response.
      response = this.createResponse(request);

      if (!request.type) {
        throw new Error('Type not specified');
      }

      if (request.type != 'request') {
        throw new Error("Illegal type '" + request.type + "' in request");
      }

      if (!request.command) {
        throw new Error('Command not specified');
      }

      // TODO(yurys): remove request.arguments.compactFormat check once
      // ChromeDevTools are switched to 'inlineRefs'
      if (request.arguments && (request.arguments.inlineRefs ||
                                request.arguments.compactFormat)) {
        response.setOption('inlineRefs', true);
      }

      if (request.command == 'continue') {
        this.continueRequest_(request, response);
      } else if (request.command == 'break') {
        this.breakRequest_(request, response);
      } else if (request.command == 'setbreakpoint') {
        this.setBreakPointRequest_(request, response);
      } else if (request.command == 'changebreakpoint') {
        this.changeBreakPointRequest_(request, response);
      } else if (request.command == 'clearbreakpoint') {
        this.clearBreakPointRequest_(request, response);
      } else if (request.command == 'backtrace') {
        this.backtraceRequest_(request, response);
      } else if (request.command == 'frame') {
        this.frameRequest_(request, response);
      } else if (request.command == 'scopes') {
        this.scopesRequest_(request, response);
      } else if (request.command == 'scope') {
        this.scopeRequest_(request, response);
      } else if (request.command == 'evaluate') {
        this.evaluateRequest_(request, response);
      } else if (request.command == 'lookup') {
        this.lookupRequest_(request, response);
      } else if (request.command == 'references') {
        this.referencesRequest_(request, response);
      } else if (request.command == 'source') {
        this.sourceRequest_(request, response);
      } else if (request.command == 'scripts') {
        this.scriptsRequest_(request, response);
      } else if (request.command == 'threads') {
        this.threadsRequest_(request, response);
      } else {
        throw new Error('Unknown command "' + request.command + '" in request');
      }
    } catch (e) {
      // If there is no response object created one (without command).
      if (!response) {
        response = this.createResponse();
      }
      response.success = false;
      response.message = %ToString(e);
    }

    // Return the response as a JSON encoded string.
    try {
      this.running_ = response.running;  // Store the running state.
      return response.toJSONProtocol();
    } catch (e) {
      // Failed to generate response - return generic error.
      return '{"seq":' + response.seq + ',' +
              '"request_seq":' + request.seq + ',' +
              '"type":"response",' +
              '"success":false,' +
              '"message":"Internal error: ' + %ToString(e) + '"}';
    }
  } catch (e) {
    // Failed in one of the catch blocks above - most generic error.
    return '{"seq":0,"type":"response","success":false,"message":"Internal error"}';
  }
};


DebugCommandProcessor.prototype.continueRequest_ = function(request, response) {
  // Check for arguments for continue.
  if (request.arguments) {
    var count = 1;
    var action = Debug.StepAction.StepIn;

    // Pull out arguments.
    var stepaction = request.arguments.stepaction;
    var stepcount = request.arguments.stepcount;

    // Get the stepcount argument if any.
    if (stepcount) {
      count = %ToNumber(stepcount);
      if (count < 0) {
        throw new Error('Invalid stepcount argument "' + stepcount + '".');
      }
    }

    // Get the stepaction argument.
    if (stepaction) {
      if (stepaction == 'in') {
        action = Debug.StepAction.StepIn;
      } else if (stepaction == 'min') {
        action = Debug.StepAction.StepMin;
      } else if (stepaction == 'next') {
        action = Debug.StepAction.StepNext;
      } else if (stepaction == 'out') {
        action = Debug.StepAction.StepOut;
      } else {
        throw new Error('Invalid stepaction argument "' + stepaction + '".');
      }
    }

    // Setup the VM for stepping.
    this.exec_state_.prepareStep(action, count);
  }

  // VM should be running after executing this request.
  response.running = true;
};


DebugCommandProcessor.prototype.breakRequest_ = function(request, response) {
  // Ignore as break command does not do anything when broken.
};


DebugCommandProcessor.prototype.setBreakPointRequest_ =
    function(request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var type = request.arguments.type;
  var target = request.arguments.target;
  var line = request.arguments.line;
  var column = request.arguments.column;
  var enabled = IS_UNDEFINED(request.arguments.enabled) ?
      true : request.arguments.enabled;
  var condition = request.arguments.condition;
  var ignoreCount = request.arguments.ignoreCount;

  // Check for legal arguments.
  if (!type || IS_UNDEFINED(target)) {
    response.failed('Missing argument "type" or "target"');
    return;
  }
  if (type != 'function' && type != 'handle' &&
      type != 'script' && type != 'scriptId') {
    response.failed('Illegal type "' + type + '"');
    return;
  }

  // Either function or script break point.
  var break_point_number;
  if (type == 'function') {
    // Handle function break point.
    if (!IS_STRING(target)) {
      response.failed('Argument "target" is not a string value');
      return;
    }
    var f;
    try {
      // Find the function through a global evaluate.
      f = this.exec_state_.evaluateGlobal(target).value();
    } catch (e) {
      response.failed('Error: "' + %ToString(e) +
                      '" evaluating "' + target + '"');
      return;
    }
    if (!IS_FUNCTION(f)) {
      response.failed('"' + target + '" does not evaluate to a function');
      return;
    }

    // Set function break point.
    break_point_number = Debug.setBreakPoint(f, line, column, condition);
  } else if (type == 'handle') {
    // Find the object pointed by the specified handle.
    var handle = parseInt(target, 10);
    var mirror = LookupMirror(handle);
    if (!mirror) {
      return response.failed('Object #' + handle + '# not found');
    }
    if (!mirror.isFunction()) {
      return response.failed('Object #' + handle + '# is not a function');
    }

    // Set function break point.
    break_point_number = Debug.setBreakPoint(mirror.value(),
                                             line, column, condition);
  } else if (type == 'script') {
    // set script break point.
    break_point_number =
        Debug.setScriptBreakPointByName(target, line, column, condition);
  } else {  // type == 'scriptId.
    break_point_number =
        Debug.setScriptBreakPointById(target, line, column, condition);
  }

  // Set additional break point properties.
  var break_point = Debug.findBreakPoint(break_point_number);
  if (ignoreCount) {
    Debug.changeBreakPointIgnoreCount(break_point_number, ignoreCount);
  }
  if (!enabled) {
    Debug.disableBreakPoint(break_point_number);
  }

  // Add the break point number to the response.
  response.body = { type: type,
                    breakpoint: break_point_number }

  // Add break point information to the response.
  if (break_point instanceof ScriptBreakPoint) {
    if (break_point.type() == Debug.ScriptBreakPointType.ScriptId) {
      response.body.type = 'scriptId';
      response.body.script_id = break_point.script_id();
    } else {
      response.body.type = 'scriptName';
      response.body.script_name = break_point.script_name();
    }
    response.body.line = break_point.line();
    response.body.column = break_point.column();
  } else {
    response.body.type = 'function';
  }
};


DebugCommandProcessor.prototype.changeBreakPointRequest_ = function(request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var break_point = %ToNumber(request.arguments.breakpoint);
  var enabled = request.arguments.enabled;
  var condition = request.arguments.condition;
  var ignoreCount = request.arguments.ignoreCount;

  // Check for legal arguments.
  if (!break_point) {
    response.failed('Missing argument "breakpoint"');
    return;
  }

  // Change enabled state if supplied.
  if (!IS_UNDEFINED(enabled)) {
    if (enabled) {
      Debug.enableBreakPoint(break_point);
    } else {
      Debug.disableBreakPoint(break_point);
    }
  }

  // Change condition if supplied
  if (!IS_UNDEFINED(condition)) {
    Debug.changeBreakPointCondition(break_point, condition);
  }

  // Change ignore count if supplied
  if (!IS_UNDEFINED(ignoreCount)) {
    Debug.changeBreakPointIgnoreCount(break_point, ignoreCount);
  }
}


DebugCommandProcessor.prototype.clearBreakPointRequest_ = function(request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var break_point = %ToNumber(request.arguments.breakpoint);

  // Check for legal arguments.
  if (!break_point) {
    response.failed('Missing argument "breakpoint"');
    return;
  }

  // Clear break point.
  Debug.clearBreakPoint(break_point);

  // Add the cleared break point number to the response.
  response.body = { breakpoint: break_point }
}


DebugCommandProcessor.prototype.backtraceRequest_ = function(request, response) {
  // Get the number of frames.
  var total_frames = this.exec_state_.frameCount();

  // Create simple response if there are no frames.
  if (total_frames == 0) {
    response.body = {
      totalFrames: total_frames
    }
    return;
  }

  // Default frame range to include in backtrace.
  var from_index = 0
  var to_index = kDefaultBacktraceLength;

  // Get the range from the arguments.
  if (request.arguments) {
    if (request.arguments.fromFrame) {
      from_index = request.arguments.fromFrame;
    }
    if (request.arguments.toFrame) {
      to_index = request.arguments.toFrame;
    }
    if (request.arguments.bottom) {
      var tmp_index = total_frames - from_index;
      from_index = total_frames - to_index
      to_index = tmp_index;
    }
    if (from_index < 0 || to_index < 0) {
      return response.failed('Invalid frame number');
    }
  }

  // Adjust the index.
  to_index = Math.min(total_frames, to_index);

  if (to_index <= from_index) {
    var error = 'Invalid frame range';
    return response.failed(error);
  }

  // Create the response body.
  var frames = [];
  for (var i = from_index; i < to_index; i++) {
    frames.push(this.exec_state_.frame(i));
  }
  response.body = {
    fromFrame: from_index,
    toFrame: to_index,
    totalFrames: total_frames,
    frames: frames
  }
};


DebugCommandProcessor.prototype.backtracec = function(cmd, args) {
  return this.exec_state_.cframesValue();
};


DebugCommandProcessor.prototype.frameRequest_ = function(request, response) {
  // No frames no source.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No frames');
  }

  // With no arguments just keep the selected frame.
  if (request.arguments) {
    var index = request.arguments.number;
    if (index < 0 || this.exec_state_.frameCount() <= index) {
      return response.failed('Invalid frame number');
    }
    
    this.exec_state_.setSelectedFrame(request.arguments.number);
  }
  response.body = this.exec_state_.frame();
};


DebugCommandProcessor.prototype.frameForScopeRequest_ = function(request) {
  // Get the frame for which the scope or scopes are requested. With no frameNumber
  // argument use the currently selected frame.
  if (request.arguments && !IS_UNDEFINED(request.arguments.frameNumber)) {
    frame_index = request.arguments.frameNumber;
    if (frame_index < 0 || this.exec_state_.frameCount() <= frame_index) {
      return response.failed('Invalid frame number');
    }
    return this.exec_state_.frame(frame_index);
  } else {
    return this.exec_state_.frame();
  }
}


DebugCommandProcessor.prototype.scopesRequest_ = function(request, response) {
  // No frames no scopes.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No scopes');
  }

  // Get the frame for which the scopes are requested.
  var frame = this.frameForScopeRequest_(request);
  
  // Fill all scopes for this frame.
  var total_scopes = frame.scopeCount();
  var scopes = [];
  for (var i = 0; i < total_scopes; i++) {
    scopes.push(frame.scope(i));
  }
  response.body = {
    fromScope: 0,
    toScope: total_scopes,
    totalScopes: total_scopes,
    scopes: scopes
  }
};


DebugCommandProcessor.prototype.scopeRequest_ = function(request, response) {
  // No frames no scopes.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No scopes');
  }

  // Get the frame for which the scope is requested.
  var frame = this.frameForScopeRequest_(request);

  // With no scope argument just return top scope.
  var scope_index = 0;
  if (request.arguments && !IS_UNDEFINED(request.arguments.number)) {
    scope_index = %ToNumber(request.arguments.number);
    if (scope_index < 0 || frame.scopeCount() <= scope_index) {
      return response.failed('Invalid scope number');
    }
  }

  response.body = frame.scope(scope_index);
};


DebugCommandProcessor.prototype.evaluateRequest_ = function(request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }

  // Pull out arguments.
  var expression = request.arguments.expression;
  var frame = request.arguments.frame;
  var global = request.arguments.global;
  var disable_break = request.arguments.disable_break;

  // The expression argument could be an integer so we convert it to a
  // string.
  try {
    expression = String(expression);
  } catch(e) {
    return response.failed('Failed to convert expression argument to string');
  }

  // Check for legal arguments.
  if (!IS_UNDEFINED(frame) && global) {
    return response.failed('Arguments "frame" and "global" are exclusive');
  }

  // Global evaluate.
  if (global) {
    // Evaluate in the global context.
    response.body =
        this.exec_state_.evaluateGlobal(expression), Boolean(disable_break);
    return;
  }

  // Default value for disable_break is true.
  if (IS_UNDEFINED(disable_break)) {
    disable_break = true;
  }

  // No frames no evaluate in frame.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No frames');
  }

  // Check whether a frame was specified.
  if (!IS_UNDEFINED(frame)) {
    var frame_number = %ToNumber(frame);
    if (frame_number < 0 || frame_number >= this.exec_state_.frameCount()) {
      return response.failed('Invalid frame "' + frame + '"');
    }
    // Evaluate in the specified frame.
    response.body = this.exec_state_.frame(frame_number).evaluate(
        expression, Boolean(disable_break));
    return;
  } else {
    // Evaluate in the selected frame.
    response.body = this.exec_state_.frame().evaluate(
        expression, Boolean(disable_break));
    return;
  }
};


DebugCommandProcessor.prototype.lookupRequest_ = function(request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }

  // Pull out arguments.
  var handles = request.arguments.handles;

  // Check for legal arguments.
  if (IS_UNDEFINED(handles)) {
    return response.failed('Argument "handles" missing');
  }

  // Set 'includeSource' option for script lookup.
  if (!IS_UNDEFINED(request.arguments.includeSource)) {
    includeSource = %ToBoolean(request.arguments.includeSource);
    response.setOption('includeSource', includeSource);
  }
  
  // Lookup handles.
  var mirrors = {};
  for (var i = 0; i < handles.length; i++) {
    var handle = handles[i];
    var mirror = LookupMirror(handle);
    if (!mirror) {
      return response.failed('Object #' + handle + '# not found');
    }
    mirrors[handle] = mirror;
  }
  response.body = mirrors;
};


DebugCommandProcessor.prototype.referencesRequest_ =
    function(request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }

  // Pull out arguments.
  var type = request.arguments.type;
  var handle = request.arguments.handle;

  // Check for legal arguments.
  if (IS_UNDEFINED(type)) {
    return response.failed('Argument "type" missing');
  }
  if (IS_UNDEFINED(handle)) {
    return response.failed('Argument "handle" missing');
  }
  if (type != 'referencedBy' && type != 'constructedBy') {
    return response.failed('Invalid type "' + type + '"');
  }

  // Lookup handle and return objects with references the object.
  var mirror = LookupMirror(handle);
  if (mirror) {
    if (type == 'referencedBy') {
      response.body = mirror.referencedBy();
    } else {
      response.body = mirror.constructedBy();
    }
  } else {
    return response.failed('Object #' + handle + '# not found');
  }
};


DebugCommandProcessor.prototype.sourceRequest_ = function(request, response) {
  // No frames no source.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No source');
  }

  var from_line;
  var to_line;
  var frame = this.exec_state_.frame();
  if (request.arguments) {
    // Pull out arguments.
    from_line = request.arguments.fromLine;
    to_line = request.arguments.toLine;

    if (!IS_UNDEFINED(request.arguments.frame)) {
      var frame_number = %ToNumber(request.arguments.frame);
      if (frame_number < 0 || frame_number >= this.exec_state_.frameCount()) {
        return response.failed('Invalid frame "' + frame + '"');
      }
      frame = this.exec_state_.frame(frame_number);
    }
  }

  // Get the script selected.
  var script = frame.func().script();
  if (!script) {
    return response.failed('No source');
  }

  // Get the source slice and fill it into the response.
  var slice = script.sourceSlice(from_line, to_line);
  if (!slice) {
    return response.failed('Invalid line interval');
  }
  response.body = {};
  response.body.source = slice.sourceText();
  response.body.fromLine = slice.from_line;
  response.body.toLine = slice.to_line;
  response.body.fromPosition = slice.from_position;
  response.body.toPosition = slice.to_position;
  response.body.totalLines = script.lineCount();
};


DebugCommandProcessor.prototype.scriptsRequest_ = function(request, response) {
  var types = ScriptTypeFlag(Debug.ScriptType.Normal);
  var includeSource = false;
  var idsToInclude = null;
  if (request.arguments) {
    // Pull out arguments.
    if (!IS_UNDEFINED(request.arguments.types)) {
      types = %ToNumber(request.arguments.types);
      if (isNaN(types) || types < 0) {
        return response.failed('Invalid types "' + request.arguments.types + '"');
      }
    }
    
    if (!IS_UNDEFINED(request.arguments.includeSource)) {
      includeSource = %ToBoolean(request.arguments.includeSource);
      response.setOption('includeSource', includeSource);
    }
    
    if (IS_ARRAY(request.arguments.ids)) {
      idsToInclude = {};
      var ids = request.arguments.ids;
      for (var i = 0; i < ids.length; i++) {
        idsToInclude[ids[i]] = true;
      }
    }
  }

  // Collect all scripts in the heap.
  var scripts = %DebugGetLoadedScripts();

  response.body = [];

  for (var i = 0; i < scripts.length; i++) {
    if (idsToInclude && !idsToInclude[scripts[i].id]) {
      continue;
    }
    if (types & ScriptTypeFlag(scripts[i].type)) {
      response.body.push(MakeMirror(scripts[i]));
    }
  }
};


DebugCommandProcessor.prototype.threadsRequest_ = function(request, response) {
  // Get the number of threads.
  var total_threads = this.exec_state_.threadCount();

  // Get information for all threads.
  var threads = [];
  for (var i = 0; i < total_threads; i++) {
    var details = %GetThreadDetails(this.exec_state_.break_id, i);
    var thread_info = { current: details[0],
                        id: details[1]
                      }
    threads.push(thread_info);
  }

  // Create the response body.
  response.body = {
    totalThreads: total_threads,
    threads: threads
  }
};


// Check whether the previously processed command caused the VM to become
// running.
DebugCommandProcessor.prototype.isRunning = function() {
  return this.running_;
}


DebugCommandProcessor.prototype.systemBreak = function(cmd, args) {
  return %SystemBreak();
};


function NumberToHex8Str(n) {
  var r = "";
  for (var i = 0; i < 8; ++i) {
    var c = hexCharArray[n & 0x0F];  // hexCharArray is defined in uri.js
    r = c + r;
    n = n >>> 4;
  }
  return r;
};

DebugCommandProcessor.prototype.formatCFrames = function(cframes_value) {
  var result = "";
  if (cframes_value == null || cframes_value.length == 0) {
    result += "(stack empty)";
  } else {
    for (var i = 0; i < cframes_value.length; ++i) {
      if (i != 0) result += "\n";
      result += this.formatCFrame(cframes_value[i]);
    }
  }
  return result;
};


DebugCommandProcessor.prototype.formatCFrame = function(cframe_value) {
  var result = "";
  result += "0x" + NumberToHex8Str(cframe_value.address);
  if (!IS_UNDEFINED(cframe_value.text)) {
    result += " " + cframe_value.text;
  }
  return result;
}


/**
 * Convert an Object to its debugger protocol representation. The representation
 * may be serilized to a JSON object using JSON.stringify().
 * This implementation simply runs through all string property names, converts
 * each property value to a protocol value and adds the property to the result
 * object. For type "object" the function will be called recursively. Note that
 * circular structures will cause infinite recursion.
 * @param {Object} object The object to format as protocol object.
 * @param {MirrorSerializer} mirror_serializer The serializer to use if any
 *     mirror objects are encountered.
 * @return {Object} Protocol object value.
 */
function ObjectToProtocolObject_(object, mirror_serializer) {
  var content = {};
  for (var key in object) {
    // Only consider string keys.
    if (typeof key == 'string') {
      // Format the value based on its type.
      var property_value_json = ValueToProtocolValue_(object[key],
                                                      mirror_serializer);
      // Add the property if relevant.
      if (!IS_UNDEFINED(property_value_json)) {
        content[key] = property_value_json;
      }
    }
  }
  
  return content;
}


/**
 * Convert an array to its debugger protocol representation. It will convert
 * each array element to a protocol value.
 * @param {Array} array The array to format as protocol array.
 * @param {MirrorSerializer} mirror_serializer The serializer to use if any
 *     mirror objects are encountered.
 * @return {Array} Protocol array value.
 */
function ArrayToProtocolArray_(array, mirror_serializer) {
  var json = [];
  for (var i = 0; i < array.length; i++) {
    json.push(ValueToProtocolValue_(array[i], mirror_serializer));
  }
  return json;
}


/**
 * Convert a value to its debugger protocol representation. 
 * @param {*} value The value to format as protocol value.
 * @param {MirrorSerializer} mirror_serializer The serializer to use if any
 *     mirror objects are encountered.
 * @return {*} Protocol value.
 */
function ValueToProtocolValue_(value, mirror_serializer) {
  // Format the value based on its type.
  var json;
  switch (typeof value) {
    case 'object':
      if (value instanceof Mirror) {
        json = mirror_serializer.serializeValue(value);
      } else if (IS_ARRAY(value)){
        json = ArrayToProtocolArray_(value, mirror_serializer);
      } else {
        json = ObjectToProtocolObject_(value, mirror_serializer);
      }
      break;

    case 'boolean':
    case 'string':
    case 'number':
      json = value;
      break

    default:
      json = null;
  }
  return json;
}
