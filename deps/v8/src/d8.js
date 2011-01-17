// Copyright 2008 the V8 project authors. All rights reserved.
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

String.prototype.startsWith = function (str) {
  if (str.length > this.length)
    return false;
  return this.substr(0, str.length) == str;
}

function log10(num) {
  return Math.log(num)/Math.log(10);
}

function ToInspectableObject(obj) {
  if (!obj && typeof obj === 'object') {
    return void 0;
  } else {
    return Object(obj);
  }
}

function GetCompletions(global, last, full) {
  var full_tokens = full.split();
  full = full_tokens.pop();
  var parts = full.split('.');
  parts.pop();
  var current = global;
  for (var i = 0; i < parts.length; i++) {
    var part = parts[i];
    var next = current[part];
    if (!next)
      return [];
    current = next;
  }
  var result = [];
  current = ToInspectableObject(current);
  while (typeof current !== 'undefined') {
    var mirror = new $debug.ObjectMirror(current);
    var properties = mirror.properties();
    for (var i = 0; i < properties.length; i++) {
      var name = properties[i].name();
      if (typeof name === 'string' && name.startsWith(last))
        result.push(name);
    }
    current = ToInspectableObject(current.__proto__);
  }
  return result;
}


// Global object holding debugger related constants and state.
const Debug = {};


// Debug events which can occour in the V8 JavaScript engine. These originate
// from the API include file v8-debug.h.
Debug.DebugEvent = { Break: 1,
                     Exception: 2,
                     NewFunction: 3,
                     BeforeCompile: 4,
                     AfterCompile: 5 };


// The different types of scripts matching enum ScriptType in objects.h.
Debug.ScriptType = { Native: 0,
                     Extension: 1,
                     Normal: 2 };


// The different types of script compilations matching enum
// Script::CompilationType in objects.h.
Debug.ScriptCompilationType = { Host: 0,
                                Eval: 1,
                                JSON: 2 };


// The different types of scopes matching constants runtime.cc.
Debug.ScopeType = { Global: 0,
                    Local: 1,
                    With: 2,
                    Closure: 3,
                    Catch: 4 };


// Current debug state.
const kNoFrame = -1;
Debug.State = {
  currentFrame: kNoFrame,
  displaySourceStartLine: -1,
  displaySourceEndLine: -1,
  currentSourceLine: -1
}
var trace_compile = false;  // Tracing all compile events?
var trace_debug_json = false; // Tracing all debug json packets?
var last_cmd_line = '';
var repeat_cmd_line = '';
var is_running = true;

// Copied from debug-delay.js.  This is needed below:
function ScriptTypeFlag(type) {
  return (1 << type);
}


// Process a debugger JSON message into a display text and a running status.
// This function returns an object with properties "text" and "running" holding
// this information.
function DebugMessageDetails(message) {
  if (trace_debug_json) {
    print("received: '" + message + "'");
  }
  // Convert the JSON string to an object.
  var response = new ProtocolPackage(message);
  is_running = response.running();

  if (response.type() == 'event') {
    return DebugEventDetails(response);
  } else {
    return DebugResponseDetails(response);
  }
}

function DebugEventDetails(response) {
  details = {text:'', running:false}

  // Get the running state.
  details.running = response.running();

  var body = response.body();
  var result = '';
  switch (response.event()) {
    case 'break':
      if (body.breakpoints) {
        result += 'breakpoint';
        if (body.breakpoints.length > 1) {
          result += 's';
        }
        result += ' #';
        for (var i = 0; i < body.breakpoints.length; i++) {
          if (i > 0) {
            result += ', #';
          }
          result += body.breakpoints[i];
        }
      } else {
        result += 'break';
      }
      result += ' in ';
      result += body.invocationText;
      result += ', ';
      result += SourceInfo(body);
      result += '\n';
      result += SourceUnderline(body.sourceLineText, body.sourceColumn);
      Debug.State.currentSourceLine = body.sourceLine;
      Debug.State.displaySourceStartLine = -1;
      Debug.State.displaySourceEndLine = -1;
      Debug.State.currentFrame = 0;
      details.text = result;
      break;

    case 'exception':
      if (body.uncaught) {
        result += 'Uncaught: ';
      } else {
        result += 'Exception: ';
      }
      result += '"';
      result += body.exception.text;
      result += '"';
      if (body.sourceLine >= 0) {
        result += ', ';
        result += SourceInfo(body);
        result += '\n';
        result += SourceUnderline(body.sourceLineText, body.sourceColumn);
        Debug.State.currentSourceLine = body.sourceLine;
        Debug.State.displaySourceStartLine = -1;
        Debug.State.displaySourceEndLine = -1;
        Debug.State.currentFrame = 0;
      } else {
        result += ' (empty stack)';
        Debug.State.currentSourceLine = -1;
        Debug.State.displaySourceStartLine = -1;
        Debug.State.displaySourceEndLine = -1;
        Debug.State.currentFrame = kNoFrame;
      }
      details.text = result;
      break;

    case 'afterCompile':
      if (trace_compile) {
        result = 'Source ' + body.script.name + ' compiled:\n'
        var source = body.script.source;
        if (!(source[source.length - 1] == '\n')) {
          result += source;
        } else {
          result += source.substring(0, source.length - 1);
        }
      }
      details.text = result;
      break;

    case 'scriptCollected':
      details.text = result;
      break;

    default:
      details.text = 'Unknown debug event ' + response.event();
  }

  return details;
};


function SourceInfo(body) {
  var result = '';

  if (body.script) {
    if (body.script.name) {
      result += body.script.name;
    } else {
      result += '[unnamed]';
    }
  }
  result += ' line ';
  result += body.sourceLine + 1;
  result += ' column ';
  result += body.sourceColumn + 1;

  return result;
}


function SourceUnderline(source_text, position) {
  if (!source_text) {
    return;
  }

  // Create an underline with a caret pointing to the source position. If the
  // source contains a tab character the underline will have a tab character in
  // the same place otherwise the underline will have a space character.
  var underline = '';
  for (var i = 0; i < position; i++) {
    if (source_text[i] == '\t') {
      underline += '\t';
    } else {
      underline += ' ';
    }
  }
  underline += '^';

  // Return the source line text with the underline beneath.
  return source_text + '\n' + underline;
};


// Converts a text command to a JSON request.
function DebugCommandToJSONRequest(cmd_line) {
  var result = new DebugRequest(cmd_line).JSONRequest();
  if (trace_debug_json && result) {
    print("sending: '" + result + "'");
  }
  return result;
};


function DebugRequest(cmd_line) {
  // If the very first character is a { assume that a JSON request have been
  // entered as a command. Converting that to a JSON request is trivial.
  if (cmd_line && cmd_line.length > 0 && cmd_line.charAt(0) == '{') {
    this.request_ = cmd_line;
    return;
  }

  // Check for a simple carriage return to repeat the last command:
  var is_repeating = false;
  if (cmd_line == '\n') {
    if (is_running) {
      cmd_line = 'break'; // Not in debugger mode, break with a frame request.
    } else {
      cmd_line = repeat_cmd_line; // use command to repeat.
      is_repeating = true;
    }
  }
  if (!is_running) { // Only save the command if in debugger mode.
    repeat_cmd_line = cmd_line;   // save last command.
  }

  // Trim string for leading and trailing whitespace.
  cmd_line = cmd_line.replace(/^\s+|\s+$/g, '');

  // Find the command.
  var pos = cmd_line.indexOf(' ');
  var cmd;
  var args;
  if (pos == -1) {
    cmd = cmd_line;
    args = '';
  } else {
    cmd = cmd_line.slice(0, pos);
    args = cmd_line.slice(pos).replace(/^\s+|\s+$/g, '');
  }

  if ((cmd === undefined) || !cmd) {
    this.request_ = void 0;
    return;
  }

  last_cmd = cmd;

  // Switch on command.
  switch (cmd) {
    case 'continue':
    case 'c':
      this.request_ = this.continueCommandToJSONRequest_(args);
      break;

    case 'step':
    case 's':
      this.request_ = this.stepCommandToJSONRequest_(args, 'in');
      break;

    case 'stepi':
    case 'si':
      this.request_ = this.stepCommandToJSONRequest_(args, 'min');
      break;

    case 'next':
    case 'n':
      this.request_ = this.stepCommandToJSONRequest_(args, 'next');
      break;

    case 'finish':
    case 'fin':
      this.request_ = this.stepCommandToJSONRequest_(args, 'out');
      break;

    case 'backtrace':
    case 'bt':
      this.request_ = this.backtraceCommandToJSONRequest_(args);
      break;

    case 'frame':
    case 'f':
      this.request_ = this.frameCommandToJSONRequest_(args);
      break;

    case 'scopes':
      this.request_ = this.scopesCommandToJSONRequest_(args);
      break;

    case 'scope':
      this.request_ = this.scopeCommandToJSONRequest_(args);
      break;

    case 'disconnect':
    case 'exit':
    case 'quit':
      this.request_ = this.disconnectCommandToJSONRequest_(args);
      break;

    case 'up':
      this.request_ =
          this.frameCommandToJSONRequest_('' +
                                          (Debug.State.currentFrame + 1));
      break;
      
    case 'down':
    case 'do':
      this.request_ =
          this.frameCommandToJSONRequest_('' +
                                          (Debug.State.currentFrame - 1));
      break;
      
    case 'set':
    case 'print':
    case 'p':
      this.request_ = this.printCommandToJSONRequest_(args);
      break;

    case 'dir':
      this.request_ = this.dirCommandToJSONRequest_(args);
      break;

    case 'references':
      this.request_ = this.referencesCommandToJSONRequest_(args);
      break;

    case 'instances':
      this.request_ = this.instancesCommandToJSONRequest_(args);
      break;

    case 'list':
    case 'l':
      this.request_ = this.listCommandToJSONRequest_(args);
      break;
    case 'source':
      this.request_ = this.sourceCommandToJSONRequest_(args);
      break;

    case 'scripts':
    case 'script':
    case 'scr':
      this.request_ = this.scriptsCommandToJSONRequest_(args);
      break;

    case 'break':
    case 'b':
      this.request_ = this.breakCommandToJSONRequest_(args);
      break;

    case 'breakpoints':
    case 'bb':
      this.request_ = this.breakpointsCommandToJSONRequest_(args);
      break;

    case 'clear':
    case 'delete':
    case 'd':
      this.request_ = this.clearCommandToJSONRequest_(args);
      break;

    case 'threads':
      this.request_ = this.threadsCommandToJSONRequest_(args);
      break;

    case 'cond':
      this.request_ = this.changeBreakpointCommandToJSONRequest_(args, 'cond');
      break;

    case 'enable':
    case 'en':
      this.request_ =
          this.changeBreakpointCommandToJSONRequest_(args, 'enable');
      break;

    case 'disable':
    case 'dis':
      this.request_ =
          this.changeBreakpointCommandToJSONRequest_(args, 'disable');
      break;

    case 'ignore':
      this.request_ =
          this.changeBreakpointCommandToJSONRequest_(args, 'ignore');
      break;

    case 'info':
    case 'inf':
      this.request_ = this.infoCommandToJSONRequest_(args);
      break;

    case 'flags':
      this.request_ = this.v8FlagsToJSONRequest_(args);
      break;

    case 'gc':
      this.request_ = this.gcToJSONRequest_(args);
      break;

    case 'trace':
    case 'tr':
      // Return undefined to indicate command handled internally (no JSON).
      this.request_ = void 0;
      this.traceCommand_(args);
      break;

    case 'help':
    case '?':
      this.helpCommand_(args);
      // Return undefined to indicate command handled internally (no JSON).
      this.request_ = void 0;
      break;

    default:
      throw new Error('Unknown command "' + cmd + '"');
  }
}

DebugRequest.prototype.JSONRequest = function() {
  return this.request_;
}


function RequestPacket(command) {
  this.seq = 0;
  this.type = 'request';
  this.command = command;
}


RequestPacket.prototype.toJSONProtocol = function() {
  // Encode the protocol header.
  var json = '{';
  json += '"seq":' + this.seq;
  json += ',"type":"' + this.type + '"';
  if (this.command) {
    json += ',"command":' + StringToJSON_(this.command);
  }
  if (this.arguments) {
    json += ',"arguments":';
    // Encode the arguments part.
    if (this.arguments.toJSONProtocol) {
      json += this.arguments.toJSONProtocol()
    } else {
      json += SimpleObjectToJSON_(this.arguments);
    }
  }
  json += '}';
  return json;
}


DebugRequest.prototype.createRequest = function(command) {
  return new RequestPacket(command);
};


// Create a JSON request for the evaluation command.
DebugRequest.prototype.makeEvaluateJSONRequest_ = function(expression) {
  // Global varaible used to store whether a handle was requested.
  lookup_handle = null;
  // Check if the expression is a handle id in the form #<handle>#.
  var handle_match = expression.match(/^#([0-9]*)#$/);
  if (handle_match) {
    // Remember the handle requested in a global variable.
    lookup_handle = parseInt(handle_match[1]);
    // Build a lookup request.
    var request = this.createRequest('lookup');
    request.arguments = {};
    request.arguments.handles = [ lookup_handle ];
    return request.toJSONProtocol();
  } else {
    // Build an evaluate request.
    var request = this.createRequest('evaluate');
    request.arguments = {};
    request.arguments.expression = expression;
    // Request a global evaluation if there is no current frame.
    if (Debug.State.currentFrame == kNoFrame) {
      request.arguments.global = true;
    }
    return request.toJSONProtocol();
  }
};


// Create a JSON request for the references/instances command.
DebugRequest.prototype.makeReferencesJSONRequest_ = function(handle, type) {
  // Build a references request.
  var handle_match = handle.match(/^#([0-9]*)#$/);
  if (handle_match) {
    var request = this.createRequest('references');
    request.arguments = {};
    request.arguments.type = type;
    request.arguments.handle = parseInt(handle_match[1]);
    return request.toJSONProtocol();
  } else {
    throw new Error('Invalid object id.');
  }
};


// Create a JSON request for the continue command.
DebugRequest.prototype.continueCommandToJSONRequest_ = function(args) {
  var request = this.createRequest('continue');
  return request.toJSONProtocol();
};


// Create a JSON request for the step command.
DebugRequest.prototype.stepCommandToJSONRequest_ = function(args, type) {
  // Requesting a step is through the continue command with additional
  // arguments.
  var request = this.createRequest('continue');
  request.arguments = {};

  // Process arguments if any.

  // Only process args if the command is 'step' which is indicated by type being
  // set to 'in'.  For all other commands, ignore the args.
  if (args && args.length > 0) {
    args = args.split(/\s+/g);

    if (args.length > 2) {
      throw new Error('Invalid step arguments.');
    }

    if (args.length > 0) {
      // Check if we have a gdb stype step command.  If so, the 1st arg would
      // be the step count.  If it's not a number, then assume that we're
      // parsing for the legacy v8 step command.
      var stepcount = Number(args[0]);
      if (stepcount == Number.NaN) {
        // No step count at arg 1.  Process as legacy d8 step command:
        if (args.length == 2) {
          var stepcount = parseInt(args[1]);
          if (isNaN(stepcount) || stepcount <= 0) {
            throw new Error('Invalid step count argument "' + args[0] + '".');
          }
          request.arguments.stepcount = stepcount;
        }

        // Get the step action.
        switch (args[0]) {
          case 'in':
          case 'i':
            request.arguments.stepaction = 'in';
            break;

          case 'min':
          case 'm':
            request.arguments.stepaction = 'min';
            break;

          case 'next':
          case 'n':
            request.arguments.stepaction = 'next';
            break;

          case 'out':
          case 'o':
            request.arguments.stepaction = 'out';
            break;

          default:
            throw new Error('Invalid step argument "' + args[0] + '".');
        }

      } else {
        // gdb style step commands:
        request.arguments.stepaction = type;
        request.arguments.stepcount = stepcount;
      }
    }
  } else {
    // Default is step of the specified type.
    request.arguments.stepaction = type;
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the backtrace command.
DebugRequest.prototype.backtraceCommandToJSONRequest_ = function(args) {
  // Build a backtrace request from the text command.
  var request = this.createRequest('backtrace');

  // Default is to show top 10 frames.
  request.arguments = {};
  request.arguments.fromFrame = 0;
  request.arguments.toFrame = 10;

  args = args.split(/\s*[ ]+\s*/g);
  if (args.length == 1 && args[0].length > 0) {
    var frameCount = parseInt(args[0]);
    if (frameCount > 0) {
      // Show top frames.
      request.arguments.fromFrame = 0;
      request.arguments.toFrame = frameCount;
    } else {
      // Show bottom frames.
      request.arguments.fromFrame = 0;
      request.arguments.toFrame = -frameCount;
      request.arguments.bottom = true;
    }
  } else if (args.length == 2) {
    var fromFrame = parseInt(args[0]);
    var toFrame = parseInt(args[1]);
    if (isNaN(fromFrame) || fromFrame < 0) {
      throw new Error('Invalid start frame argument "' + args[0] + '".');
    }
    if (isNaN(toFrame) || toFrame < 0) {
      throw new Error('Invalid end frame argument "' + args[1] + '".');
    }
    if (fromFrame > toFrame) {
      throw new Error('Invalid arguments start frame cannot be larger ' +
                      'than end frame.');
    }
    // Show frame range.
    request.arguments.fromFrame = fromFrame;
    request.arguments.toFrame = toFrame + 1;
  } else if (args.length > 2) {
    throw new Error('Invalid backtrace arguments.');
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the frame command.
DebugRequest.prototype.frameCommandToJSONRequest_ = function(args) {
  // Build a frame request from the text command.
  var request = this.createRequest('frame');
  args = args.split(/\s*[ ]+\s*/g);
  if (args.length > 0 && args[0].length > 0) {
    request.arguments = {};
    request.arguments.number = args[0];
  }
  return request.toJSONProtocol();
};


// Create a JSON request for the scopes command.
DebugRequest.prototype.scopesCommandToJSONRequest_ = function(args) {
  // Build a scopes request from the text command.
  var request = this.createRequest('scopes');
  return request.toJSONProtocol();
};


// Create a JSON request for the scope command.
DebugRequest.prototype.scopeCommandToJSONRequest_ = function(args) {
  // Build a scope request from the text command.
  var request = this.createRequest('scope');
  args = args.split(/\s*[ ]+\s*/g);
  if (args.length > 0 && args[0].length > 0) {
    request.arguments = {};
    request.arguments.number = args[0];
  }
  return request.toJSONProtocol();
};


// Create a JSON request for the print command.
DebugRequest.prototype.printCommandToJSONRequest_ = function(args) {
  // Build an evaluate request from the text command.
  if (args.length == 0) {
    throw new Error('Missing expression.');
  }
  return this.makeEvaluateJSONRequest_(args);
};


// Create a JSON request for the dir command.
DebugRequest.prototype.dirCommandToJSONRequest_ = function(args) {
  // Build an evaluate request from the text command.
  if (args.length == 0) {
    throw new Error('Missing expression.');
  }
  return this.makeEvaluateJSONRequest_(args);
};


// Create a JSON request for the references command.
DebugRequest.prototype.referencesCommandToJSONRequest_ = function(args) {
  // Build an evaluate request from the text command.
  if (args.length == 0) {
    throw new Error('Missing object id.');
  }

  return this.makeReferencesJSONRequest_(args, 'referencedBy');
};


// Create a JSON request for the instances command.
DebugRequest.prototype.instancesCommandToJSONRequest_ = function(args) {
  // Build an evaluate request from the text command.
  if (args.length == 0) {
    throw new Error('Missing object id.');
  }

  // Build a references request.
  return this.makeReferencesJSONRequest_(args, 'constructedBy');
};


// Create a JSON request for the list command.
DebugRequest.prototype.listCommandToJSONRequest_ = function(args) {

  // Default is ten lines starting five lines before the current location.
  if (Debug.State.displaySourceEndLine == -1) {
    // If we list forwards, we will start listing after the last source end
    // line.  Set it to start from 5 lines before the current location.
    Debug.State.displaySourceEndLine = Debug.State.currentSourceLine - 5;
    // If we list backwards, we will start listing backwards from the last
    // source start line.  Set it to start from 1 lines before the current
    // location.
    Debug.State.displaySourceStartLine = Debug.State.currentSourceLine + 1;
  }

  var from = Debug.State.displaySourceEndLine + 1;
  var lines = 10;

  // Parse the arguments.
  args = args.split(/\s*,\s*/g);
  if (args == '') {
  } else if ((args.length == 1) && (args[0] == '-')) {
    from = Debug.State.displaySourceStartLine - lines;
  } else if (args.length == 2) {
    from = parseInt(args[0]);
    lines = parseInt(args[1]) - from + 1; // inclusive of the ending line.
  } else {
    throw new Error('Invalid list arguments.');
  }
  Debug.State.displaySourceStartLine = from;
  Debug.State.displaySourceEndLine = from + lines - 1;
  var sourceArgs = '' + from + ' ' + lines;
  return this.sourceCommandToJSONRequest_(sourceArgs);
};


// Create a JSON request for the source command.
DebugRequest.prototype.sourceCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('source');

  // Default is ten lines starting five lines before the current location.
  var from = Debug.State.currentSourceLine - 5;
  var lines = 10;

  // Parse the arguments.
  args = args.split(/\s*[ ]+\s*/g);
  if (args.length > 1 && args[0].length > 0 && args[1].length > 0) {
    from = parseInt(args[0]) - 1;
    lines = parseInt(args[1]);
  } else if (args.length > 0 && args[0].length > 0) {
    from = parseInt(args[0]) - 1;
  }

  if (from < 0) from = 0;
  if (lines < 0) lines = 10;

  // Request source arround current source location.
  request.arguments = {};
  request.arguments.fromLine = from;
  request.arguments.toLine = from + lines;

  return request.toJSONProtocol();
};


// Create a JSON request for the scripts command.
DebugRequest.prototype.scriptsCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('scripts');

  // Process arguments if any.
  if (args && args.length > 0) {
    args = args.split(/\s*[ ]+\s*/g);

    if (args.length > 1) {
      throw new Error('Invalid scripts arguments.');
    }

    request.arguments = {};
    switch (args[0]) {
      case 'natives':
        request.arguments.types = ScriptTypeFlag(Debug.ScriptType.Native);
        break;

      case 'extensions':
        request.arguments.types = ScriptTypeFlag(Debug.ScriptType.Extension);
        break;

      case 'all':
        request.arguments.types =
            ScriptTypeFlag(Debug.ScriptType.Normal) |
            ScriptTypeFlag(Debug.ScriptType.Native) |
            ScriptTypeFlag(Debug.ScriptType.Extension);
        break;

      default:
        // If the arg is not one of the know one aboves, then it must be a
        // filter used for filtering the results:
        request.arguments.filter = args[0];
        break;
    }
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the break command.
DebugRequest.prototype.breakCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  // Process arguments if any.
  if (args && args.length > 0) {
    var target = args;
    var type = 'function';
    var line;
    var column;
    var condition;
    var pos;

    var request = this.createRequest('setbreakpoint');

    // Break the args into target spec and condition if appropriate.

    // Check for breakpoint condition.
    pos = args.indexOf(' ');
    if (pos > 0) {
      target = args.substring(0, pos);
      condition = args.substring(pos + 1, args.length);
    }

    // Check for script breakpoint (name:line[:column]). If no ':' in break
    // specification it is considered a function break point.
    pos = target.indexOf(':');
    if (pos > 0) {
      type = 'script';
      var tmp = target.substring(pos + 1, target.length);
      target = target.substring(0, pos);

      // Check for both line and column.
      pos = tmp.indexOf(':');
      if (pos > 0) {
        column = parseInt(tmp.substring(pos + 1, tmp.length)) - 1;
        line = parseInt(tmp.substring(0, pos)) - 1;
      } else {
        line = parseInt(tmp) - 1;
      }
    } else if (target[0] == '#' && target[target.length - 1] == '#') {
      type = 'handle';
      target = target.substring(1, target.length - 1);
    } else {
      type = 'function';
    }

    request.arguments = {};
    request.arguments.type = type;
    request.arguments.target = target;
    request.arguments.line = line;
    request.arguments.column = column;
    request.arguments.condition = condition;
  } else {
    var request = this.createRequest('suspend');
  }

  return request.toJSONProtocol();
};


DebugRequest.prototype.breakpointsCommandToJSONRequest_ = function(args) {
  if (args && args.length > 0) {
    throw new Error('Unexpected arguments.');
  }
  var request = this.createRequest('listbreakpoints');
  return request.toJSONProtocol();
};


// Create a JSON request for the clear command.
DebugRequest.prototype.clearCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('clearbreakpoint');

  // Process arguments if any.
  if (args && args.length > 0) {
    request.arguments = {};
    request.arguments.breakpoint = parseInt(args);
  } else {
    throw new Error('Invalid break arguments.');
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the change breakpoint command.
DebugRequest.prototype.changeBreakpointCommandToJSONRequest_ =
    function(args, command) {

  var request;

  // Check for exception breaks first:
  //   en[able] exc[eptions] [all|unc[aught]]
  //   en[able] [all|unc[aught]] exc[eptions]
  //   dis[able] exc[eptions] [all|unc[aught]]
  //   dis[able] [all|unc[aught]] exc[eptions]
  if ((command == 'enable' || command == 'disable') &&
      args && args.length > 1) {
    var nextPos = args.indexOf(' ');
    var arg1 = (nextPos > 0) ? args.substring(0, nextPos) : args;
    var excType = null;

    // Check for:
    //   en[able] exc[eptions] [all|unc[aught]]
    //   dis[able] exc[eptions] [all|unc[aught]]
    if (arg1 == 'exc' || arg1 == 'exception' || arg1 == 'exceptions') {

      var arg2 = (nextPos > 0) ?
          args.substring(nextPos + 1, args.length) : 'all';
      if (!arg2) {
        arg2 = 'all'; // if unspecified, set for all.
      } if (arg2 == 'unc') { // check for short cut.
        arg2 = 'uncaught';
      }
      excType = arg2;
      
    // Check for:
    //   en[able] [all|unc[aught]] exc[eptions]
    //   dis[able] [all|unc[aught]] exc[eptions]
    } else if (arg1 == 'all' || arg1 == 'unc' || arg1 == 'uncaught') {

      var arg2 = (nextPos > 0) ?
          args.substring(nextPos + 1, args.length) : null;
      if (arg2 == 'exc' || arg1 == 'exception' || arg1 == 'exceptions') {
        excType = arg1;
        if (excType == 'unc') {
          excType = 'uncaught';
        }
      }
    }

    // If we matched one of the command formats, then excType will be non-null:
    if (excType) {
      // Build a evaluate request from the text command.
      request = this.createRequest('setexceptionbreak');

      request.arguments = {};
      request.arguments.type = excType;
      request.arguments.enabled = (command == 'enable');

      return request.toJSONProtocol();
    }
  }

  // Build a evaluate request from the text command.
  request = this.createRequest('changebreakpoint');

  // Process arguments if any.
  if (args && args.length > 0) {
    request.arguments = {};
    var pos = args.indexOf(' ');
    var breakpointArg = args;
    var otherArgs;
    if (pos > 0) {
      breakpointArg = args.substring(0, pos);
      otherArgs = args.substring(pos + 1, args.length);
    }

    request.arguments.breakpoint = parseInt(breakpointArg);

    switch(command) {
      case 'cond':
        request.arguments.condition = otherArgs ? otherArgs : null;
        break;
      case 'enable':
        request.arguments.enabled = true;
        break;
      case 'disable':
        request.arguments.enabled = false;
        break;
      case 'ignore':
        request.arguments.ignoreCount = parseInt(otherArgs);
        break;
      default:
        throw new Error('Invalid arguments.');  
    }
  } else {
    throw new Error('Invalid arguments.');
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the disconnect command.
DebugRequest.prototype.disconnectCommandToJSONRequest_ = function(args) {
  var request;
  request = this.createRequest('disconnect');
  return request.toJSONProtocol();
};


// Create a JSON request for the info command.
DebugRequest.prototype.infoCommandToJSONRequest_ = function(args) {
  var request;
  if (args && (args == 'break' || args == 'br')) {
    // Build a evaluate request from the text command.
    request = this.createRequest('listbreakpoints');
    last_cmd = 'info break';
  } else if (args && (args == 'locals' || args == 'lo')) {
    // Build a evaluate request from the text command.
    request = this.createRequest('frame');
    last_cmd = 'info locals';
  } else if (args && (args == 'args' || args == 'ar')) {
    // Build a evaluate request from the text command.
    request = this.createRequest('frame');
    last_cmd = 'info args';
  } else {
    throw new Error('Invalid info arguments.');
  }

  return request.toJSONProtocol();
};


DebugRequest.prototype.v8FlagsToJSONRequest_ = function(args) {
  var request;
  request = this.createRequest('v8flags');
  request.arguments = {};
  request.arguments.flags = args;
  return request.toJSONProtocol();
};


DebugRequest.prototype.gcToJSONRequest_ = function(args) {
  var request;
  if (!args) {
    args = 'all';
  }
  var args = args.split(/\s+/g);
  var cmd = args[0];

  switch(cmd) {
    case 'all':
    case 'quick':
    case 'full':
    case 'young':
    case 'old':
    case 'compact':
    case 'sweep':
    case 'scavenge': {
      if (cmd == 'young') { cmd = 'quick'; }
      else if (cmd == 'old') { cmd = 'full'; }

      request = this.createRequest('gc');
      request.arguments = {};
      request.arguments.type = cmd;
      break;
    }
      // Else fall thru to the default case below to report the error.
    default:
      throw new Error('Missing arguments after ' + cmd + '.');
  }
  return request.toJSONProtocol();
};


// Create a JSON request for the threads command.
DebugRequest.prototype.threadsCommandToJSONRequest_ = function(args) {
  // Build a threads request from the text command.
  var request = this.createRequest('threads');
  return request.toJSONProtocol();
};


// Handle the trace command.
DebugRequest.prototype.traceCommand_ = function(args) {
  // Process arguments.
  if (args && args.length > 0) {
    if (args == 'compile') {
      trace_compile = !trace_compile;
      print('Tracing of compiled scripts ' + (trace_compile ? 'on' : 'off'));
    } else if (args === 'debug json' || args === 'json' || args === 'packets') {
      trace_debug_json = !trace_debug_json;
      print('Tracing of debug json packets ' +
            (trace_debug_json ? 'on' : 'off'));
    } else {
      throw new Error('Invalid trace arguments.');
    }
  } else {
    throw new Error('Invalid trace arguments.');
  }
}

// Handle the help command.
DebugRequest.prototype.helpCommand_ = function(args) {
  // Help os quite simple.
  if (args && args.length > 0) {
    print('warning: arguments to \'help\' are ignored');
  }

  print('Note: <> denotes symbollic values to be replaced with real values.');
  print('Note: [] denotes optional parts of commands, or optional options / arguments.');
  print('      e.g. d[elete] - you get the same command if you type d or delete.');
  print('');
  print('[break] - break as soon as possible');
  print('b[reak] location [condition]');
  print('        - break on named function: location is a function name');
  print('        - break on function: location is #<id>#');
  print('        - break on script position: location is name:line[:column]');
  print('');
  print('clear <breakpoint #>       - deletes the specified user defined breakpoint');
  print('d[elete]  <breakpoint #>   - deletes the specified user defined breakpoint');
  print('dis[able] <breakpoint #>   - disables the specified user defined breakpoint');
  print('dis[able] exc[eptions] [[all] | unc[aught]]');
  print('                           - disables breaking on exceptions');
  print('en[able]  <breakpoint #>   - enables the specified user defined breakpoint');
  print('en[able]  exc[eptions] [[all] | unc[aught]]');
  print('                           - enables breaking on exceptions');
  print('');
  print('b[ack]t[race] [n] | [-n] | [from to]');
  print('                           - prints the stack back trace');
  print('f[rame]                    - prints info about the current frame context');
  print('f[rame] <frame #>          - set context to specified frame #');
  print('scopes');
  print('scope <scope #>');
  print('');
  print('up                         - set context to caller of current frame');
  print('do[wn]                     - set context to callee of current frame');
  print('inf[o] br[eak]             - prints info about breakpoints in use');
  print('inf[o] ar[gs]              - prints info about arguments of the current function');
  print('inf[o] lo[cals]            - prints info about locals in the current function');
  print('inf[o] liveobjectlist|lol  - same as \'lol info\'');
  print('');
  print('step [in | next | out| min [step count]]');
  print('c[ontinue]                 - continue executing after a breakpoint');
  print('s[tep]   [<N>]             - step into the next N callees (default N is 1)');
  print('s[tep]i  [<N>]             - step into the next N callees (default N is 1)');
  print('n[ext]   [<N>]             - step over the next N callees (default N is 1)');
  print('fin[ish] [<N>]             - step out of N frames (default N is 1)');
  print('');
  print('p[rint] <expression>       - prints the result of the specified expression');
  print('dir <expression>           - prints the object structure of the result');
  print('set <var> = <expression>   - executes the specified statement');
  print('');
  print('l[ist]                     - list the source code around for the current pc');
  print('l[ist] [- | <start>,<end>] - list the specified range of source code');
  print('source [from line [num lines]]');
  print('scr[ipts] [native|extensions|all]');
  print('scr[ipts] [<filter text>]  - list scripts with the specified text in its description');
  print('');
  print('gc                         - runs the garbage collector');
  print('');
  print('trace compile');
  // hidden command: trace debug json - toggles tracing of debug json packets
  print('');
  print('disconnect|exit|quit       - disconnects and quits the debugger');
  print('help                       - prints this help information');
}


function formatHandleReference_(value) {
  if (value.handle() >= 0) {
    return '#' + value.handle() + '#';
  } else {
    return '#Transient#';
  }
}


function formatObject_(value, include_properties) {
  var result = '';
  result += formatHandleReference_(value);
  result += ', type: object'
  result += ', constructor ';
  var ctor = value.constructorFunctionValue();
  result += formatHandleReference_(ctor);
  result += ', __proto__ ';
  var proto = value.protoObjectValue();
  result += formatHandleReference_(proto);
  result += ', ';
  result += value.propertyCount();
  result +=  ' properties.';
  if (include_properties) {
    result +=  '\n';
    for (var i = 0; i < value.propertyCount(); i++) {
      result += '  ';
      result += value.propertyName(i);
      result += ': ';
      var property_value = value.propertyValue(i);
      if (property_value instanceof ProtocolReference) {
        result += '<no type>';
      } else {
        if (property_value && property_value.type()) {
          result += property_value.type();
        } else {
          result += '<no type>';
        }
      }
      result += ' ';
      result += formatHandleReference_(property_value);
      result += '\n';
    }
  }
  return result;
}


function formatScope_(scope) {
  var result = '';
  var index = scope.index;
  result += '#' + (index <= 9 ? '0' : '') + index;
  result += ' ';
  switch (scope.type) {
    case Debug.ScopeType.Global:
      result += 'Global, ';
      result += '#' + scope.object.ref + '#';
      break;
    case Debug.ScopeType.Local:
      result += 'Local';
      break;
    case Debug.ScopeType.With:
      result += 'With, ';
      result += '#' + scope.object.ref + '#';
      break;
    case Debug.ScopeType.Catch:
      result += 'Catch, ';
      result += '#' + scope.object.ref + '#';
      break;
    case Debug.ScopeType.Closure:
      result += 'Closure';
      break;
    default:
      result += 'UNKNOWN';
  }
  return result;
}


function refObjectToString_(protocolPackage, handle) {
  var value = protocolPackage.lookup(handle);
  var result = '';
  if (value.isString()) {
    result = '"' + value.value() + '"';
  } else if (value.isPrimitive()) {
    result = value.valueString();
  } else if (value.isObject()) {
    result += formatObject_(value, true);
  }
  return result;
}


// Rounds number 'num' to 'length' decimal places.
function roundNumber(num, length) {
  var factor = Math.pow(10, length);
  return Math.round(num * factor) / factor;
}


// Convert a JSON response to text for display in a text based debugger.
function DebugResponseDetails(response) {
  details = {text:'', running:false}

  try {
    if (!response.success()) {
      details.text = response.message();
      return details;
    }

    // Get the running state.
    details.running = response.running();

    var body = response.body();
    var result = '';
    switch (response.command()) {
      case 'suspend':
        details.text = 'stopped';
        break;

      case 'setbreakpoint':
        result = 'set breakpoint #';
        result += body.breakpoint;
        details.text = result;
        break;

      case 'clearbreakpoint':
        result = 'cleared breakpoint #';
        result += body.breakpoint;
        details.text = result;
        break;

      case 'changebreakpoint':
        result = 'successfully changed breakpoint';
        details.text = result;
        break;

      case 'listbreakpoints':
        result = 'breakpoints: (' + body.breakpoints.length + ')';
        for (var i = 0; i < body.breakpoints.length; i++) {
          var breakpoint = body.breakpoints[i];
          result += '\n id=' + breakpoint.number;
          result += ' type=' + breakpoint.type;
          if (breakpoint.script_id) {
              result += ' script_id=' + breakpoint.script_id;
          }
          if (breakpoint.script_name) {
              result += ' script_name=' + breakpoint.script_name;
          }
          result += ' line=' + (breakpoint.line + 1);
          if (breakpoint.column != null) {
            result += ' column=' + (breakpoint.column + 1);
          }
          if (breakpoint.groupId) {
            result += ' groupId=' + breakpoint.groupId;
          }
          if (breakpoint.ignoreCount) {
              result += ' ignoreCount=' + breakpoint.ignoreCount;
          }
          if (breakpoint.active === false) {
            result += ' inactive';
          }
          if (breakpoint.condition) {
            result += ' condition=' + breakpoint.condition;
          }
          result += ' hit_count=' + breakpoint.hit_count;
        }
        if (body.breakpoints.length === 0) {
          result = "No user defined breakpoints\n";
        } else {
          result += '\n';
        }
        if (body.breakOnExceptions) {
          result += '* breaking on ALL exceptions is enabled\n';
        } else if (body.breakOnUncaughtExceptions) {
          result += '* breaking on UNCAUGHT exceptions is enabled\n';
        } else {
          result += '* all exception breakpoints are disabled\n';            
        }
        details.text = result;
        break;

      case 'setexceptionbreak':
        result = 'Break on ' + body.type + ' exceptions: ';
        result += body.enabled ? 'enabled' : 'disabled';
        details.text = result;
        break;

      case 'backtrace':
        if (body.totalFrames == 0) {
          result = '(empty stack)';
        } else {
          var result = 'Frames #' + body.fromFrame + ' to #' +
              (body.toFrame - 1) + ' of ' + body.totalFrames + '\n';
          for (i = 0; i < body.frames.length; i++) {
            if (i != 0) result += '\n';
            result += body.frames[i].text;
          }
        }
        details.text = result;
        break;

      case 'frame':
        if (last_cmd === 'info locals') {
          var locals = body.locals;
          if (locals.length === 0) {
            result = 'No locals';
          } else {
            for (var i = 0; i < locals.length; i++) {
              var local = locals[i];
              result += local.name + ' = ';
              result += refObjectToString_(response, local.value.ref);
              result += '\n';
            }
          }
        } else if (last_cmd === 'info args') {
          var args = body.arguments;
          if (args.length === 0) {
            result = 'No arguments';
          } else {
            for (var i = 0; i < args.length; i++) {
              var arg = args[i];
              result += arg.name + ' = ';
              result += refObjectToString_(response, arg.value.ref);
              result += '\n';
            }
          }
        } else {
          result = SourceUnderline(body.sourceLineText,
                                   body.column);
          Debug.State.currentSourceLine = body.line;
          Debug.State.currentFrame = body.index;
          Debug.State.displaySourceStartLine = -1;
          Debug.State.displaySourceEndLine = -1;
        }
        details.text = result;
        break;

      case 'scopes':
        if (body.totalScopes == 0) {
          result = '(no scopes)';
        } else {
          result = 'Scopes #' + body.fromScope + ' to #' +
                   (body.toScope - 1) + ' of ' + body.totalScopes + '\n';
          for (i = 0; i < body.scopes.length; i++) {
            if (i != 0) {
              result += '\n';
            }
            result += formatScope_(body.scopes[i]);
          }
        }
        details.text = result;
        break;

      case 'scope':
        result += formatScope_(body);
        result += '\n';
        var scope_object_value = response.lookup(body.object.ref);
        result += formatObject_(scope_object_value, true);
        details.text = result;
        break;

      case 'evaluate':
      case 'lookup':
        if (last_cmd == 'p' || last_cmd == 'print') {
          result = body.text;
        } else {
          var value;
          if (lookup_handle) {
            value = response.bodyValue(lookup_handle);
          } else {
            value = response.bodyValue();
          }
          if (value.isObject()) {
            result += formatObject_(value, true);
          } else {
            result += 'type: ';
            result += value.type();
            if (!value.isUndefined() && !value.isNull()) {
              result += ', ';
              if (value.isString()) {
                result += '"';
              }
              result += value.value();
              if (value.isString()) {
                result += '"';
              }
            }
            result += '\n';
          }
        }
        details.text = result;
        break;

      case 'references':
        var count = body.length;
        result += 'found ' + count + ' objects';
        result += '\n';
        for (var i = 0; i < count; i++) {
          var value = response.bodyValue(i);
          result += formatObject_(value, false);
          result += '\n';
        }
        details.text = result;
        break;

      case 'source':
        // Get the source from the response.
        var source = body.source;
        var from_line = body.fromLine + 1;
        var lines = source.split('\n');
        var maxdigits = 1 + Math.floor(log10(from_line + lines.length));
        if (maxdigits < 3) {
          maxdigits = 3;
        }
        var result = '';
        for (var num = 0; num < lines.length; num++) {
          // Check if there's an extra newline at the end.
          if (num == (lines.length - 1) && lines[num].length == 0) {
            break;
          }

          var current_line = from_line + num;
          spacer = maxdigits - (1 + Math.floor(log10(current_line)));
          if (current_line == Debug.State.currentSourceLine + 1) {
            for (var i = 0; i < maxdigits; i++) {
              result += '>';
            }
            result += '  ';
          } else {
            for (var i = 0; i < spacer; i++) {
              result += ' ';
            }
            result += current_line + ': ';
          }
          result += lines[num];
          result += '\n';
        }
        details.text = result;
        break;

      case 'scripts':
        var result = '';
        for (i = 0; i < body.length; i++) {
          if (i != 0) result += '\n';
          if (body[i].id) {
            result += body[i].id;
          } else {
            result += '[no id]';
          }
          result += ', ';
          if (body[i].name) {
            result += body[i].name;
          } else {
            if (body[i].compilationType == Debug.ScriptCompilationType.Eval
                && body[i].evalFromScript
                ) {
              result += 'eval from ';
              var script_value = response.lookup(body[i].evalFromScript.ref);
              result += ' ' + script_value.field('name');
              result += ':' + (body[i].evalFromLocation.line + 1);
              result += ':' + body[i].evalFromLocation.column;
            } else if (body[i].compilationType ==
                       Debug.ScriptCompilationType.JSON) {
              result += 'JSON ';
            } else {  // body[i].compilation == Debug.ScriptCompilationType.Host
              result += '[unnamed] ';
            }
          }
          result += ' (lines: ';
          result += body[i].lineCount;
          result += ', length: ';
          result += body[i].sourceLength;
          if (body[i].type == Debug.ScriptType.Native) {
            result += ', native';
          } else if (body[i].type == Debug.ScriptType.Extension) {
            result += ', extension';
          }
          result += '), [';
          var sourceStart = body[i].sourceStart;
          if (sourceStart.length > 40) {
            sourceStart = sourceStart.substring(0, 37) + '...';
          }
          result += sourceStart;
          result += ']';
        }
        if (body.length == 0) {
          result = "no matching scripts found";
        }
        details.text = result;
        break;

      case 'threads':
        var result = 'Active V8 threads: ' + body.totalThreads + '\n';
        body.threads.sort(function(a, b) { return a.id - b.id; });
        for (i = 0; i < body.threads.length; i++) {
          result += body.threads[i].current ? '*' : ' ';
          result += ' ';
          result += body.threads[i].id;
          result += '\n';
        }
        details.text = result;
        break;

      case 'continue':
        details.text = "(running)";
        break;

      case 'v8flags':
        details.text = "flags set";
        break;

      case 'gc':
        details.text = "GC " + body.before + " => " + body.after;
        if (body.after > (1024*1024)) {
          details.text +=
              " (" + roundNumber(body.before/(1024*1024), 1) + "M => " +
                     roundNumber(body.after/(1024*1024), 1) + "M)";
        } else if (body.after > 1024) {
          details.text +=
              " (" + roundNumber(body.before/1024, 1) + "K => " +
                     roundNumber(body.after/1024, 1) + "K)";
        }
        break;

      default:
        details.text =
            'Response for unknown command \'' + response.command() + '\'' +
            ' (' + response.raw_json() + ')';
    }
  } catch (e) {
    details.text = 'Error: "' + e + '" formatting response';
  }

  return details;
};


/**
 * Protocol packages send from the debugger.
 * @param {string} json - raw protocol packet as JSON string.
 * @constructor
 */
function ProtocolPackage(json) {
  this.raw_json_ = json;
  this.packet_ = JSON.parse(json);
  this.refs_ = [];
  if (this.packet_.refs) {
    for (var i = 0; i < this.packet_.refs.length; i++) {
      this.refs_[this.packet_.refs[i].handle] = this.packet_.refs[i];
    }
  }
}


/**
 * Get the packet type.
 * @return {String} the packet type
 */
ProtocolPackage.prototype.type = function() {
  return this.packet_.type;
}


/**
 * Get the packet event.
 * @return {Object} the packet event
 */
ProtocolPackage.prototype.event = function() {
  return this.packet_.event;
}


/**
 * Get the packet request sequence.
 * @return {number} the packet request sequence
 */
ProtocolPackage.prototype.requestSeq = function() {
  return this.packet_.request_seq;
}


/**
 * Get the packet request sequence.
 * @return {number} the packet request sequence
 */
ProtocolPackage.prototype.running = function() {
  return this.packet_.running ? true : false;
}


ProtocolPackage.prototype.success = function() {
  return this.packet_.success ? true : false;
}


ProtocolPackage.prototype.message = function() {
  return this.packet_.message;
}


ProtocolPackage.prototype.command = function() {
  return this.packet_.command;
}


ProtocolPackage.prototype.body = function() {
  return this.packet_.body;
}


ProtocolPackage.prototype.bodyValue = function(index) {
  if (index != null) {
    return new ProtocolValue(this.packet_.body[index], this);
  } else {
    return new ProtocolValue(this.packet_.body, this);
  }
}


ProtocolPackage.prototype.body = function() {
  return this.packet_.body;
}


ProtocolPackage.prototype.lookup = function(handle) {
  var value = this.refs_[handle];
  if (value) {
    return new ProtocolValue(value, this);
  } else {
    return new ProtocolReference(handle);
  }
}


ProtocolPackage.prototype.raw_json = function() {
  return this.raw_json_;
}


function ProtocolValue(value, packet) {
  this.value_ = value;
  this.packet_ = packet;
}


/**
 * Get the value type.
 * @return {String} the value type
 */
ProtocolValue.prototype.type = function() {
  return this.value_.type;
}


/**
 * Get a metadata field from a protocol value.
 * @return {Object} the metadata field value
 */
ProtocolValue.prototype.field = function(name) {
  return this.value_[name];
}


/**
 * Check is the value is a primitive value.
 * @return {boolean} true if the value is primitive
 */
ProtocolValue.prototype.isPrimitive = function() {
  return this.isUndefined() || this.isNull() || this.isBoolean() ||
         this.isNumber() || this.isString();
}


/**
 * Get the object handle.
 * @return {number} the value handle
 */
ProtocolValue.prototype.handle = function() {
  return this.value_.handle;
}


/**
 * Check is the value is undefined.
 * @return {boolean} true if the value is undefined
 */
ProtocolValue.prototype.isUndefined = function() {
  return this.value_.type == 'undefined';
}


/**
 * Check is the value is null.
 * @return {boolean} true if the value is null
 */
ProtocolValue.prototype.isNull = function() {
  return this.value_.type == 'null';
}


/**
 * Check is the value is a boolean.
 * @return {boolean} true if the value is a boolean
 */
ProtocolValue.prototype.isBoolean = function() {
  return this.value_.type == 'boolean';
}


/**
 * Check is the value is a number.
 * @return {boolean} true if the value is a number
 */
ProtocolValue.prototype.isNumber = function() {
  return this.value_.type == 'number';
}


/**
 * Check is the value is a string.
 * @return {boolean} true if the value is a string
 */
ProtocolValue.prototype.isString = function() {
  return this.value_.type == 'string';
}


/**
 * Check is the value is an object.
 * @return {boolean} true if the value is an object
 */
ProtocolValue.prototype.isObject = function() {
  return this.value_.type == 'object' || this.value_.type == 'function' ||
         this.value_.type == 'error' || this.value_.type == 'regexp';
}


/**
 * Get the constructor function
 * @return {ProtocolValue} constructor function
 */
ProtocolValue.prototype.constructorFunctionValue = function() {
  var ctor = this.value_.constructorFunction;
  return this.packet_.lookup(ctor.ref);
}


/**
 * Get the __proto__ value
 * @return {ProtocolValue} __proto__ value
 */
ProtocolValue.prototype.protoObjectValue = function() {
  var proto = this.value_.protoObject;
  return this.packet_.lookup(proto.ref);
}


/**
 * Get the number og properties.
 * @return {number} the number of properties
 */
ProtocolValue.prototype.propertyCount = function() {
  return this.value_.properties ? this.value_.properties.length : 0;
}


/**
 * Get the specified property name.
 * @return {string} property name
 */
ProtocolValue.prototype.propertyName = function(index) {
  var property = this.value_.properties[index];
  return property.name;
}


/**
 * Return index for the property name.
 * @param name The property name to look for
 * @return {number} index for the property name
 */
ProtocolValue.prototype.propertyIndex = function(name) {
  for (var i = 0; i < this.propertyCount(); i++) {
    if (this.value_.properties[i].name == name) {
      return i;
    }
  }
  return null;
}


/**
 * Get the specified property value.
 * @return {ProtocolValue} property value
 */
ProtocolValue.prototype.propertyValue = function(index) {
  var property = this.value_.properties[index];
  return this.packet_.lookup(property.ref);
}


/**
 * Check is the value is a string.
 * @return {boolean} true if the value is a string
 */
ProtocolValue.prototype.value = function() {
  return this.value_.value;
}


ProtocolValue.prototype.valueString = function() {
  return this.value_.text;
}


function ProtocolReference(handle) {
  this.handle_ = handle;
}


ProtocolReference.prototype.handle = function() {
  return this.handle_;
}


function MakeJSONPair_(name, value) {
  return '"' + name + '":' + value;
}


function ArrayToJSONObject_(content) {
  return '{' + content.join(',') + '}';
}


function ArrayToJSONArray_(content) {
  return '[' + content.join(',') + ']';
}


function BooleanToJSON_(value) {
  return String(value);
}


function NumberToJSON_(value) {
  return String(value);
}


// Mapping of some control characters to avoid the \uXXXX syntax for most
// commonly used control cahracters.
const ctrlCharMap_ = {
  '\b': '\\b',
  '\t': '\\t',
  '\n': '\\n',
  '\f': '\\f',
  '\r': '\\r',
  '"' : '\\"',
  '\\': '\\\\'
};


// Regular expression testing for ", \ and control characters (0x00 - 0x1F).
const ctrlCharTest_ = new RegExp('["\\\\\x00-\x1F]');


// Regular expression matching ", \ and control characters (0x00 - 0x1F)
// globally.
const ctrlCharMatch_ = new RegExp('["\\\\\x00-\x1F]', 'g');


/**
 * Convert a String to its JSON representation (see http://www.json.org/). To
 * avoid depending on the String object this method calls the functions in
 * string.js directly and not through the value.
 * @param {String} value The String value to format as JSON
 * @return {string} JSON formatted String value
 */
function StringToJSON_(value) {
  // Check for" , \ and control characters (0x00 - 0x1F). No need to call
  // RegExpTest as ctrlchar is constructed using RegExp.
  if (ctrlCharTest_.test(value)) {
    // Replace ", \ and control characters (0x00 - 0x1F).
    return '"' +
      value.replace(ctrlCharMatch_, function (char) {
        // Use charmap if possible.
        var mapped = ctrlCharMap_[char];
        if (mapped) return mapped;
        mapped = char.charCodeAt();
        // Convert control character to unicode escape sequence.
        return '\\u00' +
          '0' + // TODO %NumberToRadixString(Math.floor(mapped / 16), 16) +
          '0' // TODO %NumberToRadixString(mapped % 16, 16);
      })
    + '"';
  }

  // Simple string with no special characters.
  return '"' + value + '"';
}


/**
 * Convert a Date to ISO 8601 format. To avoid depending on the Date object
 * this method calls the functions in date.js directly and not through the
 * value.
 * @param {Date} value The Date value to format as JSON
 * @return {string} JSON formatted Date value
 */
function DateToISO8601_(value) {
  function f(n) {
    return n < 10 ? '0' + n : n;
  }
  function g(n) {
    return n < 10 ? '00' + n : n < 100 ? '0' + n : n;
  }
  return builtins.GetUTCFullYearFrom(value)         + '-' +
          f(builtins.GetUTCMonthFrom(value) + 1)    + '-' +
          f(builtins.GetUTCDateFrom(value))         + 'T' +
          f(builtins.GetUTCHoursFrom(value))        + ':' +
          f(builtins.GetUTCMinutesFrom(value))      + ':' +
          f(builtins.GetUTCSecondsFrom(value))      + '.' +
          g(builtins.GetUTCMillisecondsFrom(value)) + 'Z';
}


/**
 * Convert a Date to ISO 8601 format. To avoid depending on the Date object
 * this method calls the functions in date.js directly and not through the
 * value.
 * @param {Date} value The Date value to format as JSON
 * @return {string} JSON formatted Date value
 */
function DateToJSON_(value) {
  return '"' + DateToISO8601_(value) + '"';
}


/**
 * Convert an Object to its JSON representation (see http://www.json.org/).
 * This implementation simply runs through all string property names and adds
 * each property to the JSON representation for some predefined types. For type
 * "object" the function calls itself recursively unless the object has the
 * function property "toJSONProtocol" in which case that is used. This is not
 * a general implementation but sufficient for the debugger. Note that circular
 * structures will cause infinite recursion.
 * @param {Object} object The object to format as JSON
 * @return {string} JSON formatted object value
 */
function SimpleObjectToJSON_(object) {
  var content = [];
  for (var key in object) {
    // Only consider string keys.
    if (typeof key == 'string') {
      var property_value = object[key];

      // Format the value based on its type.
      var property_value_json;
      switch (typeof property_value) {
        case 'object':
          if (property_value === null) {
            property_value_json = 'null';
          } else if (typeof property_value.toJSONProtocol == 'function') {
            property_value_json = property_value.toJSONProtocol(true)
          } else if (property_value.constructor.name == 'Array'){
            property_value_json = SimpleArrayToJSON_(property_value);
          } else {
            property_value_json = SimpleObjectToJSON_(property_value);
          }
          break;

        case 'boolean':
          property_value_json = BooleanToJSON_(property_value);
          break;

        case 'number':
          property_value_json = NumberToJSON_(property_value);
          break;

        case 'string':
          property_value_json = StringToJSON_(property_value);
          break;

        default:
          property_value_json = null;
      }

      // Add the property if relevant.
      if (property_value_json) {
        content.push(StringToJSON_(key) + ':' + property_value_json);
      }
    }
  }

  // Make JSON object representation.
  return '{' + content.join(',') + '}';
}


/**
 * Convert an array to its JSON representation. This is a VERY simple
 * implementation just to support what is needed for the debugger.
 * @param {Array} arrya The array to format as JSON
 * @return {string} JSON formatted array value
 */
function SimpleArrayToJSON_(array) {
  // Make JSON array representation.
  var json = '[';
  for (var i = 0; i < array.length; i++) {
    if (i != 0) {
      json += ',';
    }
    var elem = array[i];
    if (elem.toJSONProtocol) {
      json += elem.toJSONProtocol(true)
    } else if (typeof(elem) === 'object')  {
      json += SimpleObjectToJSON_(elem);
    } else if (typeof(elem) === 'boolean')  {
      json += BooleanToJSON_(elem);
    } else if (typeof(elem) === 'number')  {
      json += NumberToJSON_(elem);
    } else if (typeof(elem) === 'string')  {
      json += StringToJSON_(elem);
    } else {
      json += elem;
    }
  }
  json += ']';
  return json;
}
