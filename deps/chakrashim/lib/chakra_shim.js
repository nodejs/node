// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
'use strict';

// CHAKRA-TODO doesn't implement the debugger. So add a dummy 'Debug' on
// global object for now.
Object.defineProperty(this, 'Debug',
     { value: {}, enumerable: false, configurable: false, writable: false });

(function() {
  // Save original builtIns
  var Object_defineProperty = Object.defineProperty,
      Object_getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor,
      Object_getOwnPropertyNames = Object.getOwnPropertyNames,
      Object_keys = Object.keys,
      Map_keys = Map.prototype.keys,
      Map_values = Map.prototype.values,
      Map_entries = Map.prototype.entries,
      Set_entries = Set.prototype.entries,
      Set_values = Set.prototype.values;

  // Simulate V8 JavaScript stack trace API
  function StackFrame(funcName, fileName, lineNumber, columnNumber) {
    this.column = columnNumber;
    this.lineNumber = lineNumber;
    this.scriptName = fileName;
    this.functionName = funcName;
  }

  StackFrame.prototype.getFunctionName = function() {
    return this.functionName;
  };

  StackFrame.prototype.getFileName = function() {
    return this.scriptName;
  };

  StackFrame.prototype.getLineNumber = function() {
    return this.lineNumber;
  };

  StackFrame.prototype.getColumnNumber = function() {
    return this.column;
  };

  StackFrame.prototype.isEval = function() {
    // TODO
    return false;
  };

  StackFrame.prototype.toString = function() {
    return (this.functionName || 'Anonymous function') + ' (' +
      this.scriptName + ':' + this.lineNumber + ':' + this.column + ')';
  };

  function prepareStackTrace(error, stack) {
    var stackString = (error.name ? error.name : 'Error') +
      ': ' + error.message;

    for (var i = 0; i < stack.length; i++) {
      stackString += '\n   at ' + stack[i].toString();
    }

    return stackString;
  }

  function parseStack(stack, skipDepth, startFunc) {
    // remove the first line so this function won't be seen
    var splittedStack = stack.split('\n');
    splittedStack.splice(0, 2);
    var errstack = [];

    var startName = skipDepth < 0 ? startFunc.name : undefined;
    skipDepth = Math.max(0, skipDepth);

    for (var i = skipDepth; i < splittedStack.length; i++) {
      var parens = /\(/.exec(splittedStack[i]);
      var funcName = splittedStack[i].substr(6, parens.index - 7);

      if (startName) {
        if (funcName === startName) {
          startName = undefined;
        }
        continue;
      }
      if (funcName === 'Anonymous function') {
        funcName = null;
      }

      var location = splittedStack[i].substr(parens.index + 1,
          splittedStack[i].length - parens.index - 2);

      var fileName = location;
      var lineNumber = 0;
      var columnNumber = 0;

      var colonPattern = /:[0-9]+/g;
      var firstColon = colonPattern.exec(location);
      if (firstColon) {
        fileName = location.substr(0, firstColon.index);

        var secondColon = colonPattern.exec(location);
        if (secondColon) {
          lineNumber = parseInt(location.substr(firstColon.index + 1,
              secondColon.index - firstColon.index - 1), 10);
          columnNumber = parseInt(location.substr(secondColon.index + 1,
              location.length - secondColon.index), 10);
        }
      }
      errstack.push(
          new StackFrame(funcName, fileName, lineNumber, columnNumber));
    }
    return errstack;
  }

  function findFuncDepth(func) {
    try {
      var curr = captureStackTrace.caller;
      var limit = Error.stackTraceLimit;
      var skipDepth = 0;
      while (curr) {
        skipDepth++;
        if (curr === func) {
          return skipDepth;
        }
        if (skipDepth > limit) {
          return 0;
        }
        curr = curr.caller;
      }
    } catch (e) {
      // Strict mode may throw on .caller. Will try to match by function name.
      return -1;
    }

    return 0;
  }

  function captureStackTrace(err, func) {
    var currentStack;
    try { throw new Error(); } catch (e) { currentStack = e.stack; }
    var isPrepared = false;
    var skipDepth = func ? findFuncDepth(func) : 0;

    var currentStackTrace;
    function ensureStackTrace() {
      if (!currentStackTrace) {
        currentStackTrace = parseStack(currentStack, skipDepth, func);
      }
      return currentStackTrace;
    }

    Object_defineProperty(err, 'stack', {
      get: function() {
        if (isPrepared) {
          return currentStack;
        }
        var errstack = ensureStackTrace();
        if (Error.prepareStackTrace) {
          currentStack = Error.prepareStackTrace(err, errstack);
        } else {
          currentStack = prepareStackTrace(err, errstack);
        }
        isPrepared = true;
        return currentStack;
      },
      set: function(value) {
        currentStack = value;
        isPrepared = true;
      },
      configurable: true,
      enumerable: false
    });

    return ensureStackTrace;
  };

  function patchErrorStack() {
    Error.captureStackTrace = captureStackTrace;
  }

  var mapIteratorProperty = 'MapIteratorIndicator';
  function patchMapIterator() {
    var originalMapMethods = [];
    originalMapMethods.push(['entries', Map_entries]);
    originalMapMethods.push(['values', Map_values]);
    originalMapMethods.push(['keys', Map_keys]);

    originalMapMethods.forEach(function(pair) {
      Map.prototype[pair[0]] = function() {
        var result = pair[1].apply(this);
        Object_defineProperty(result, mapIteratorProperty,
          { value: true, enumerable: false, writable: false });
        return result;
      };
    });
  }

  var setIteratorProperty = 'SetIteratorIndicator';
  function patchSetIterator() {
    var originalSetMethods = [];
    originalSetMethods.push(['entries', Set_entries]);
    originalSetMethods.push(['values', Set_values]);

    originalSetMethods.forEach(function(pair) {
      Set.prototype[pair[0]] = function() {
        var result = pair[1].apply(this);
        Object_defineProperty(result, setIteratorProperty,
          { value: true, enumerable: false, writable: false });
        return result;
      };
    });
  }

  function patchUtils(utils) {
    var isUintRegex = /^(0|[1-9]\\d*)$/;

    var isUint = function(value) {
      var result = isUintRegex.test(value);
      isUintRegex.lastIndex = 0;
      return result;
    };
    utils.cloneObject = function(source, target) {
      Object_getOwnPropertyNames(source).forEach(function(key) {
        try {
          var desc = Object_getOwnPropertyDescriptor(source, key);
          if (desc.value === source) desc.value = target;
          Object_defineProperty(target, key, desc);
        } catch (e) {
          // Catch sealed properties errors
        }
      });
    };
    utils.getPropertyNames = function(a) {
      var names = [];
      for (var propertyName in a) {
        names.push(propertyName);
      }
      return names;
    };
    utils.getEnumerableNamedProperties = function(obj) {
      var props = [];
      for (var key in obj) {
        if (!isUint(key))
          props.push(key);
      }
      return props;
    };
    utils.getEnumerableIndexedProperties = function(obj) {
      var props = [];
      for (var key in obj) {
        if (isUint(key))
          props.push(key);
      }
      return props;
    };
    utils.createEnumerationIterator = function(props) {
      var i = 0;
      return {
        next: function() {
          if (i === props.length)
            return { done: true };
          return { value: props[i++] };
        }
      };
    };
    utils.createPropertyDescriptorsEnumerationIterator = function(props) {
      var i = 0;
      return {
        next: function() {
          if (i === props.length) return { done: true };
          return { name: props[i++], enumerable: true };
        }
      };
    };
    utils.getNamedOwnKeys = function(obj) {
      var props = [];
      Object_keys(obj).forEach(function(item) {
        if (!isUint(item))
          props.push(item);
      });
      return props;
    };
    utils.getIndexedOwnKeys = function(obj) {
      var props = [];
      Object_keys(obj).forEach(function(item) {
        if (isUint(item))
          props.push(item);
      });
      return props;
    };
    utils.getStackTrace = function() {
      return captureStackTrace({}, utils.getStackTrace)();
    };
    utils.isMapIterator = function(value) {
      return value[mapIteratorProperty] == true;
    };
    utils.isSetIterator = function(value) {
      return value[setIteratorProperty] == true;
    };
  }

  // patch console
  patchErrorStack();

  // patch map iterators
  patchMapIterator();

  // patch set iterators
  patchSetIterator();

  // this is the keepAlive object that we will put some utilities function on
  patchUtils(this);
});
