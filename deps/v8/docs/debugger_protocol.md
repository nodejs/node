# Introduction

V8 has support for debugging the JavaScript code running in it. There are two API's for this a function based API using JavaScript objects and a message based API using a JSON based protocol. The function based API can be used by an in-process debugger agent, whereas the message based API can be used out of process as well.
**> The message based API is no longer maintained. Please ask in v8-users@googlegroups.com if you want to attach a debugger to the run-time.**

The debugger protocol is based on [JSON](http://www.json.org/)). Each protocol packet is defined in terms of JSON and is transmitted as a string value. All packets have two basic elements `seq` and `type`.

```
{ "seq"     : <number>,
  "type"    : <type>,
  ...
}
```

The element `seq` holds the sequence number of the packet. And element type is the type of the packet. The type is a string value with one of the following values `"request"`, `"response"` or `"event"`.

A `"request"` packet has the following structure:

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : <command>
  "arguments" : ...
}
```

A `"response"` packet has the following structure. If `success` is true `body` will contain the response data. If `success` is false `message` will contain an error message.

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : <command>
  "body"        : ...
  "running"     : <is the VM running after sending this response>
  "success"     : <boolean indicating success>
  "message"     : <if command failed this property contains an error message>
}
```

An `"event"` packet has the following structure:

```
{ "seq"     : <number>,
  "type"    : "event",
  "event"   : <event name>
  body      : ...
}
```

# Request/response pairs

## Request `continue`

The request `continue` is a request from the debugger to start the VM running again. As part of the `continue` request the debugger can specify if it wants the VM to perform a single step action.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "continue",
  "arguments" : { "stepaction" : <"in", "next" or "out">,
                  "stepcount"  : <number of steps (default 1)>
                }
}
```

In the response the property `running` will always be true as the VM will be running after executing the `continue` command. If a single step action is requested the VM will respond with a `break` event after running the step.

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "continue",
  "running"     : true
  "success"     : true
}
```


Here are a couple of examples.

```
{"seq":117,"type":"request","command":"continue"}
{"seq":118,"type":"request","command":"continue","arguments":{"stepaction":"out"}}
{"seq":119,"type":"request","command":"continue","arguments":{"stepaction":"next","stepcount":5}}
```

## Request `evaluate`

The request `evaluate` is used to evaluate an expression. The body of the result is as described in response object serialization below.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "evaluate",
  "arguments" : { "expression"    : <expression to evaluate>,
                  "frame"         : <number>,
                  "global"        : <boolean>,
                  "disable_break" : <boolean>,
                  "additional_context" : [
                       { "name" : <name1>, "handle" : <handle1> },
                       { "name" : <name2>, "handle" : <handle2> },
                       ... 
                  ]
                }
}
```

Optional argument `additional_context` specifies handles that will be visible from the expression under corresponding names (see example below).

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "evaluate",
  "body"        : ...
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"evaluate","arguments":{"expression":"1+2"}}
{"seq":118,"type":"request","command":"evaluate","arguments":{"expression":"a()","frame":3,"disable_break":false}}
{"seq":119,"type":"request","command":"evaluate","arguments":{"expression":"[o.a,o.b,o.c]","global":true,"disable_break":true}}
{"seq":120,"type":"request","command":"evaluate","arguments":{"expression":"obj.toString()", "additional_context": [{ "name":"obj","handle":25 }] }}
```

## Request `lookup`

The request `lookup` is used to lookup objects based on their handle. The individual array elements of the body of the result is as described in response object serialization below.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "lookup",
  "arguments" : { "handles"       : <array of handles>,
                  "includeSource" : <boolean indicating whether the source will be included when script objects are returned>,
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "lookup",
  "body"        : <array of serialized objects indexed using their handle>
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"lookup","arguments":{"handles":"[1]"}}
{"seq":118,"type":"request","command":"lookup","arguments":{"handles":"[7,12]"}}
```

## Request `backtrace`

The request `backtrace` returns a backtrace (or stacktrace) from the current execution state. When issuing a request a range of frames can be supplied. The top frame is frame number 0. If no frame range is supplied data for 10 frames will be returned.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "backtrace",
  "arguments" : { "fromFrame" : <number>
                  "toFrame" : <number>
                  "bottom" : <boolean, set to true if the bottom of the stack is requested>
                }
}
```

The response contains the frame data together with the actual frames returned and the toalt frame count.

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "backtrace",
  "body"        : { "fromFrame" : <number>
                    "toFrame" : <number>
                    "totalFrames" : <number>
                    "frames" : <array of frames - see frame request for details>
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

If there are no stack frames the result body only contains `totalFrames` with a value of `0`. When an exception event is generated due to compilation failures it is possible that there are no stack frames.

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"backtrace"}
{"seq":118,"type":"request","command":"backtrace","arguments":{"toFrame":2}}
{"seq":119,"type":"request","command":"backtrace","arguments":{"fromFrame":0,"toFrame":9}}
```

## Request `frame`

The request frame selects a new selected frame and returns information for that. If no frame number is specified the selected frame is returned.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "frame",
  "arguments" : { "number" : <frame number>
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "frame",
  "body"        : { "index"          : <frame number>,
                    "receiver"       : <frame receiver>,
                    "func"           : <function invoked>,
                    "script"         : <script for the function>,
                    "constructCall"  : <boolean indicating whether the function was called as constructor>,
                    "debuggerFrame"  : <boolean indicating whether this is an internal debugger frame>,
                    "arguments"      : [ { name: <name of the argument - missing of anonymous argument>,
                                           value: <value of the argument>
                                         },
                                         ... <the array contains all the arguments>
                                       ],
                    "locals"         : [ { name: <name of the local variable>,
                                           value: <value of the local variable>
                                         },
                                         ... <the array contains all the locals>
                                       ],
                    "position"       : <source position>,
                    "line"           : <source line>,
                    "column"         : <source column within the line>,
                    "sourceLineText" : <text for current source line>,
                    "scopes"         : [ <array of scopes, see scope request below for format> ],

                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"frame"}
{"seq":118,"type":"request","command":"frame","arguments":{"number":1}}
```

## Request `scope`

The request scope returns information on a givne scope for a givne frame. If no frame number is specified the selected frame is used.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "scope",
  "arguments" : { "number" : <scope number>
                  "frameNumber" : <frame number, optional uses selected frame if missing>
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "scope",
  "body"        : { "index"      : <index of this scope in the scope chain. Index 0 is the top scope
                                    and the global scope will always have the highest index for a
                                    frame>,
                    "frameIndex" : <index of the frame>,
                    "type"       : <type of the scope:
                                     0: Global
                                     1: Local
                                     2: With
                                     3: Closure
                                     4: Catch >,
                    "object"     : <the scope object defining the content of the scope.
                                    For local and closure scopes this is transient objects,
                                    which has a negative handle value>
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"scope"}
{"seq":118,"type":"request","command":"scope","arguments":{"frameNumber":1,"number":1}}
```

## Request `scopes`

The request scopes returns all the scopes for a given frame. If no frame number is specified the selected frame is returned.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "scopes",
  "arguments" : { "frameNumber" : <frame number, optional uses selected frame if missing>
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "scopes",
  "body"        : { "fromScope" : <number of first scope in response>,
                    "toScope"   : <number of last scope in response>,
                    "totalScopes" : <total number of scopes for this frame>,
                    "scopes" : [ <array of scopes, see scope request above for format> ],
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"scopes"}
{"seq":118,"type":"request","command":"scopes","arguments":{"frameNumber":1}}
```

## Request `scripts`

The request `scripts` retrieves active scripts from the VM. An active script is source code from which there is still live objects in the VM. This request will always force a full garbage collection in the VM.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "scripts",
  "arguments" : { "types"         : <types of scripts to retrieve
                                       set bit 0 for native scripts
                                       set bit 1 for extension scripts
                                       set bit 2 for normal scripts
                                     (default is 4 for normal scripts)>
                  "ids"           : <array of id's of scripts to return. If this is not specified all scripts are requrned>
                  "includeSource" : <boolean indicating whether the source code should be included for the scripts returned>
                  "filter"        : <string or number: filter string or script id.
                                     If a number is specified, then only the script with the same number as its script id will be retrieved.
                                     If a string is specified, then only scripts whose names contain the filter string will be retrieved.>
                }
}
```

The request contains an array of the scripts in the VM. This information includes the relative location of the script within the containing resource.

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "scripts",
  "body"        : [ { "name"             : <name of the script>,
                      "id"               : <id of the script>
                      "lineOffset"       : <line offset within the containing resource>
                      "columnOffset"     : <column offset within the containing resource>
                      "lineCount"        : <number of lines in the script>
                      "data"             : <optional data object added through the API>
                      "source"           : <source of the script if includeSource was specified in the request>
                      "sourceStart"      : <first 80 characters of the script if includeSource was not specified in the request>
                      "sourceLength"     : <total length of the script in characters>
                      "scriptType"       : <script type (see request for values)>
                      "compilationType"  : < How was this script compiled:
                                               0 if script was compiled through the API
                                               1 if script was compiled through eval
                                            >
                      "evalFromScript"   : <if "compilationType" is 1 this is the script from where eval was called>
                      "evalFromLocation" : { line   : < if "compilationType" is 1 this is the line in the script from where eval was called>
                                             column : < if "compilationType" is 1 this is the column in the script from where eval was called>
                  ]
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"scripts"}
{"seq":118,"type":"request","command":"scripts","arguments":{"types":7}}
```

## Request `source`

The request `source` retrieves source code for a frame. It returns a number of source lines running from the `fromLine` to but not including the `toLine`, that is the interval is open on the "to" end. For example, requesting source from line 2 to 4 returns two lines (2 and 3). Also note that the line numbers are 0 based: the first line is line 0.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "source",
  "arguments" : { "frame"    : <frame number (default selected frame)>
                  "fromLine" : <from line within the source default is line 0>
                  "toLine"   : <to line within the source this line is not included in
                                the result default is the number of lines in the script>
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "source",
  "body"        : { "source"       : <the source code>
                    "fromLine"     : <actual from line within the script>
                    "toLine"       : <actual to line within the script this line is not included in the source>
                    "fromPosition" : <actual start position within the script>
                    "toPosition"   : <actual end position within the script>
                    "totalLines"   : <total lines in the script>
                }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"source","arguments":{"fromLine":10,"toLine":20}}
{"seq":118,"type":"request","command":"source","arguments":{"frame":2,"fromLine":10,"toLine":20}}
```

## Request `setbreakpoint`

The request `setbreakpoint` creates a new break point. This request can be used to set both function and script break points. A function break point sets a break point in an existing function whereas a script break point sets a break point in a named script. A script break point can be set even if the named script is not found.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "setbreakpoint",
  "arguments" : { "type"        : <"function" or "script" or "scriptId" or "scriptRegExp">
                  "target"      : <function expression or script identification>
                  "line"        : <line in script or function>
                  "column"      : <character position within the line>
                  "enabled"     : <initial enabled state. True or false, default is true>
                  "condition"   : <string with break point condition>
                  "ignoreCount" : <number specifying the number of break point hits to ignore, default value is 0>
                }
}
```

The result of the `setbreakpoint` request is a response with the number of the newly created break point. This break point number is used in the `changebreakpoint` and `clearbreakpoint` requests.

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "setbreakpoint",
  "body"        : { "type"       : <"function" or "script">
                    "breakpoint" : <break point number of the new break point>
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"setbreakpoint","arguments":{"type":"function,"target":"f"}}
{"seq":118,"type":"request","command":"setbreakpoint","arguments":{type:"script","target":"test.js","line":100}}
{"seq":119,"type":"request","command":"setbreakpoint","arguments":{"type":"function,"target":"f","condition":"i > 7"}}
```


## Request `changebreakpoint`

The request `changebreakpoint` changes the status of a break point.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "changebreakpoint",
  "arguments" : { "breakpoint"  : <number of the break point to clear>
                  "enabled"     : <initial enabled state. True or false, default is true>
                  "condition"   : <string with break point condition>
                  "ignoreCount" : <number specifying the number of break point hits            }
}
```

## Request `clearbreakpoint`

The request `clearbreakpoint` clears a break point.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "clearbreakpoint",
  "arguments" : { "breakpoint" : <number of the break point to clear>
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "clearbreakpoint",
  "body"        : { "type"       : <"function" or "script">
                    "breakpoint" : <number of the break point cleared>
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here are a couple of examples.

```
{"seq":117,"type":"request","command":"clearbreakpoint","arguments":{"type":"function,"breakpoint":1}}
{"seq":118,"type":"request","command":"clearbreakpoint","arguments":{"type":"script","breakpoint":2}}
```

## Request `setexceptionbreak`

The request `setexceptionbreak` is a request to enable/disable breaks on all / uncaught exceptions.  If the "enabled" argument is not specify, the debuggee will toggle the state of the specified break type.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "setexceptionbreak",
  "arguments" : { "type"    : <string: "all", or "uncaught">,
                  "enabled" : <optional bool: enables the break type if true>
                }
}
```

In response, the break on exception property of the debuggee will be set accordingly, and the following response message will be dispatched to the debugger.

```
{ "seq"               : <number>,
  "type"              : "response",
  "request_seq" : <number>,
  "command"     : "setexceptionbreak",
  “body”        : { "type"    : <string: "all" or "uncaught" corresponding to the request.>,
                    "enabled" : <bool: true if the break type is currently enabled as a result of the request>
                  }
  "running"     : true
  "success"     : true
}
```

Here are a few examples.

```
{"seq":117,"type":"request","command":"setexceptionbreak","arguments":{"type":"all"}}
{"seq":118,"type":"request","command":" setexceptionbreak","arguments":{"type":"all",”enabled”:false}}
{"seq":119,"type":"request","command":" setexceptionbreak","arguments":{"type":"uncaught","enabled":true}}
```

## Request `v8flags`
The request v8flags is a request to apply the specified v8 flags (analogous to how they are specified on the command line).

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "v8flags",
  "arguments" : { "flags" : <string: a sequence of v8 flags just like those used on the command line>
                }
}
```

In response, the specified flags will be applied in the debuggee if they are legal flags.  Their effects vary depending on the implementation of the flag.

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "v8flags",
  "running"     : true
  "success"     : true
}
```

Here are a few examples.

```
{"seq":117,"type":"request","command":"v8flags","arguments":{"flags":"--trace_gc —always_compact"}}
{"seq":118,"type":"request","command":" v8flags","arguments":{"flags":"--notrace_gc"}}
```

## Request `version`

The request `version` reports version of the running V8.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "version",
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "type"        : "request",
  "body"        : { "V8Version":   <string, version of V8>
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here is an example.

```
{"seq":1,"type":"request","command":"version"}
{"seq":134,"request_seq":1,"type":"response","command":"version","success":true,"body":{"V8Version":"1.3.19 (candidate)"},"refs":[],"running":false}
```

## Request `disconnect`

The request `disconnect` is used to detach the remote debugger from the debuggee.  This will trigger the debuggee to disable all active breakpoints and resumes execution if the debuggee was previously stopped at a break.

```
{ "seq"     : <number>,
  "type"    : "request",
  "command" : "disconnect",
}
```

The only response for the `disconnect` request is the response to a connect request if the debugger is still able to get a response before the debuggee successfully disconnects.

Here is an examples:

```
{"seq":117,"type":"request","command":"disconnect"}
```

## Request `gc`
The request `gc` is a request to run the garbage collector in the debuggee.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "gc",
  "arguments" : { "type" : <string: "all">,
                }
}
```

In response, the debuggee will run the specified GC type and send the following response message:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "gc",
  “body”        : { "before" : <int: total heap usage in bytes before the GC>,
                    "after"  : <int: total heap usage in bytes after the GC>
                  }
  "running"     : true
  "success"     : true
}
```

Here is an example.

```
{"seq":117,"type":"request","command":"gc","arguments":{"type":"all"}}
```

## Request `listbreakpoints`

The request `listbreakpoints` is used to get information on breakpoints that may have been set by the debugger.

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "listbreakpoints",
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "command"     : "listbreakpoints",
  "body"        : { "breakpoints": [ { "type"             : <string: "scriptId"  or "scriptName".>,
                                       "script_id"        : <int: script id.  Only defined if type is scriptId.>,
                                       "script_name"      : <string: script name.  Only defined if type is scriptName.>,
                                       "number"           : <int: breakpoint number.  Starts from 1.>,
                                       "line"             : <int: line number of this breakpoint.  Starts from 0.>,
                                       "column"           : <int: column number of this breakpoint.  Starts from 0.>,
                                       "groupId"          : <int: group id of this breakpoint.>,
                                       "hit_count"        : <int: number of times this breakpoint has been hit.  Starts from 0.>,
                                       "active"           : <bool: true if this breakpoint is enabled.>,
                                       "ignoreCount"      : <int: remaining number of times to ignore breakpoint.  Starts from 0.>,
                                       "actual_locations" : <actual locations of the breakpoint.>,
                                     }
                                   ],
                    "breakOnExceptions"         : <true if break on all exceptions is enabled>,
                    "breakOnUncaughtExceptions" : <true if break on uncaught exceptions is enabled>
                  }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

Here is an examples:

```
{"seq":117,"type":"request","command":"listbreakpoints"}
```


## Request `setvariablevalue`
This requests sets the value of a variable from the specified scope.

Request:

```
{ "seq"       : <number>,
  "type"      : "request",
  "command"   : "setvariablevalue",
  "arguments  : { "name"  : <string: variable name>,
                  "scope" : { "number" : <scope number>
                              "frameNumber" : <frame number, optional uses selected frame if missing>
                            }
                }
}
```

Response:

```
{ "seq"         : <number>,
  "type"        : "response",
  "request_seq" : <number>,
  "type"        : "request",
  "body"        : { "newValue":   <object: mirror object of the new value> }
  "running"     : <is the VM running after sending this response>
  "success"     : true
}
```

# Events

## Event `break`

The event `break` indicate that the execution in the VM has stopped due to a break condition. This can be caused by an unconditional break request, by a break point previously set, a stepping action have completed or by executing the `debugger` statement in JavaScript.

```
{ "seq"   : <number>,
  "type"  : "event",

  "event" : "break",
  "body"  : { "invocationText" : <text representation of the stack frame>,
              "sourceLine"     : <source line where execution is stopped>,
              "sourceColumn"   : <column within the source line where execution is stopped>,
              "sourceLineText" : <text for the source line where execution is stopped>,
              "script"         : { name         : <resource name of the origin of the script>
                                   lineOffset   : <line offset within the origin of the script>
                                   columnOffset : <column offset within the origin of the script>
                                   lineCount    : <number of lines in the script>
              "breakpoints"    : <array of break point numbers hit if any>
            }
}
```

Here are a couple of examples.

```
{"seq":117,"type":"event","event":"break","body":{"functionName":"f","sourceLine":1,"sourceColumn":14}}
{"seq":117,"type":"event","event":"break","body":{"functionName":"g","scriptData":"test.js","sourceLine":12,"sourceColumn":22,"breakpoints":[1]}}
{"seq":117,"type":"event","event":"break","body":{"functionName":"h","sourceLine":100,"sourceColumn":12,"breakpoints":[3,5,7]}}
```

## Event `exception`

The event `exception` indicate that the execution in the VM has stopped due to an exception.

```
{ "seq"   : <number>,
  "type"  : "event",
  "event" : "exception",
  "body"  : { "uncaught" : <boolean>,
              "exception" : ...
              "sourceLine"     : <source line where the exception was thrown>,
              "sourceColumn"   : <column within the source line from where the exception was thrown>,
              "sourceLineText" : <text for the source line from where the exception was thrown>,
              "script" : { "name" : <name of script>
                           "lineOffset" : <number>
                           "columnOffset" : <number>
                           "lineCount" : <number>
                         }

            }
}
```

# Response object serialization

Some responses contain objects as part of the body, e.g. the response to the evaluate request contains the result of the expression evaluated.

All objects exposed through the debugger is assigned an ID called a handle. This handle is serialized and can be used to identify objects. A handle has a certain lifetime after which it will no longer refer to the same object. Currently the lifetime of handles match the processing of a debug event. For each debug event handles are recycled.

An object can be serialized either as a reference to a given handle or as a value representation containing the object content.

An object serialized as a reference looks follows this where `<handle>` is an integer.

```
{"ref":<handle>}
```

For objects serialized as value they all contains the handle and the type of the object.

```
{ "handle" : <handle>,
  "type"   : <"undefined", "null", "boolean", "number", "string", "object", "function" or "frame">
}
```

In some situations special transient objects are created by the debugger. These objects are not really visible in from JavaScript, but are created to materialize something inside the VM as an object visible to the debugger. One example of this is the local scope object returned from the `scope` and `scopes` request. Transient objects are identified by having a negative handle. A transient object can never be retrieved using the `lookup` request, so all transient objects referenced will be in the `refs` part of the response. The lifetime of transient objects is basically the request they are involved in.

For the primitive JavaScript types `undefined` and `null` the type describes the value fully.

```
{"handle":<handle>,"type":"undefined"}
```

```
{"handle":<handle>,"type":"null"}
```

For the rest of the primitive types `boolean`, `number` and `string` the value is part of the result.

```
{ "handle":<handle>,
  "type"  : <"boolean", "number" or "string">
  "value" : <JSON encoded value>
}
```

Boolean value.

```
{"handle":7,"type":"boolean","value":true}
```

Number value.

```
{"handle":8,"type":"number","value":42}
```

String value.

```
{"handle":9,"type":"string","value":"a string"}
```

An object is encoded with additional information.

```
{ "handle"              : <handle>,
  "type"                : "object",
  "className"           : <Class name, ECMA-262 property [[Class]]>,
  "constructorFunction" : {"ref":<handle>},
  "protoObject"         : {"ref":<handle>},
  "prototypeObject"     : {"ref":<handle>},
  "properties" : [ {"name" : <name>,
                    "ref"  : <handle>
                   },
                   ...
                 ]
}
```

The difference between the `protoObject` and the `prototypeObject` is that the `protoObject` contains a reference to the actual prototype object (for which accessibility is not defined in ECMA-262, but in V8 it is accessible using the `__proto__` property) whereas the `prototypeObject` is the value of the `prototype` property.

Here is an example.

```
{"handle":3,"type":"object","className":"Object","constructorFunction":{"ref":4},"protoObject":{"ref":5},"prototypeObject":{"ref":6},"properties":[{"name":"a","ref:7},{"name":"b","ref":8}]}
```

An function is encoded as an object but with additional information in the properties `name`, `inferredName`, `source` and `script`.

```
{ "handle" : <handle>,
  "type"                : "function",
  "className"           : "Function",
  "constructorFunction" : {"ref":<handle>},
  "protoObject"         : {"ref":<handle>},
  "prototypeObject"     : {"ref":<handle>},
  "name"                : <function name>,
  "inferredName"        : <inferred function name for anonymous functions>
  "source"              : <function source>,
  "script"              : <reference to function script>,
  "scriptId"            : <id of function script>,
  "position"            : <function begin position in script>,
  "line"                : <function begin source line in script>,
  "column"              : <function begin source column in script>,
  "properties" : [ {"name" : <name>,
                    "ref"  : <handle>
                   },
                   ...
                 ]
}
```