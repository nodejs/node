---
title: 'Stack trace API'
description: 'This document outlines V8’s JavaScript stack trace API.'
---
All internal errors thrown in V8 capture a stack trace when they are created. This stack trace can be accessed from JavaScript through the non-standard `error.stack` property. V8 also has various hooks for controlling how stack traces are collected and formatted, and for allowing custom errors to also collect stack traces. This document outlines V8’s JavaScript stack trace API.

## Basic stack traces

By default, almost all errors thrown by V8 have a `stack` property that holds the topmost 10 stack frames, formatted as a string. Here’s an example of a fully formatted stack trace:

```
ReferenceError: FAIL is not defined
   at Constraint.execute (deltablue.js:525:2)
   at Constraint.recalculate (deltablue.js:424:21)
   at Planner.addPropagate (deltablue.js:701:6)
   at Constraint.satisfy (deltablue.js:184:15)
   at Planner.incrementalAdd (deltablue.js:591:21)
   at Constraint.addConstraint (deltablue.js:162:10)
   at Constraint.BinaryConstraint (deltablue.js:346:7)
   at Constraint.EqualityConstraint (deltablue.js:515:38)
   at chainTest (deltablue.js:807:6)
   at deltaBlue (deltablue.js:879:2)
```

The stack trace is collected when the error is created and is the same regardless of where or how many times the error is thrown. We collect 10 frames because it is usually enough to be useful but not so many that it has a noticeable negative performance impact. You can control how many stack frames are collected by setting the variable

```js
Error.stackTraceLimit
```

Setting it to `0` disables stack trace collection. Any finite integer value can be used as the maximum number of frames to collect. Setting it to `Infinity` means that all frames get collected. This variable only affects the current context; it has to be set explicitly for each context that needs a different value. (Note that what is known as a “context” in V8 terminology corresponds to a page or `<iframe>` in Google Chrome). To set a different default value that affects all contexts use the following V8 command-line flag:

```bash
--stack-trace-limit <value>
```

To pass this flag to V8 when running Google Chrome, use:

```bash
--js-flags='--stack-trace-limit <value>'
```

## Async stack traces

The `--async-stack-traces` flag (turned on by default since [V8 v7.3](https://v8.dev/blog/v8-release-73#async-stack-traces)) enables the new [zero-cost async stack traces](https://bit.ly/v8-zero-cost-async-stack-traces), which enriches the `stack` property of `Error` instances with async stack frames, i.e. `await` locations in the code. These async frames are marked with `async` in the `stack` string:

```
ReferenceError: FAIL is not defined
    at bar (<anonymous>)
    at async foo (<anonymous>)
```

At the time of this writing, this functionality is limited to `await` locations, `Promise.all()` and `Promise.any()`, since for those cases the engine can reconstruct the necessary information without any additional overhead (that’s why it’s zero-cost).

## Stack trace collection for custom exceptions

The stack trace mechanism used for built-in errors is implemented using a general stack trace collection API that is also available to user scripts. The function

```js
Error.captureStackTrace(error, constructorOpt)
```

adds a stack property to the given `error` object that yields the stack trace at the time `captureStackTrace` was called. Stack traces collected through `Error.captureStackTrace` are immediately collected, formatted, and attached to the given `error` object.

The optional `constructorOpt` parameter allows you to pass in a function value. When collecting the stack trace all frames above the topmost call to this function, including that call, are left out of the stack trace. This can be useful to hide implementation details that won’t be useful to the user. The usual way of defining a custom error that captures a stack trace would be:

```js
function MyError() {
  Error.captureStackTrace(this, MyError);
  // Any other initialization goes here.
}
```

Passing in MyError as a second argument means that the constructor call to MyError won’t show up in the stack trace.

## Customizing stack traces

Unlike Java where the stack trace of an exception is a structured value that allows inspection of the stack state, the stack property in V8 just holds a flat string containing the formatted stack trace. This is for no other reason than compatibility with other browsers. However, this is not hardcoded but only the default behavior and can be overridden by user scripts.

For efficiency stack traces are not formatted when they are captured but on demand, the first time the stack property is accessed. A stack trace is formatted by calling

```js
Error.prepareStackTrace(error, structuredStackTrace)
```

and using whatever this call returns as the value of the `stack` property. If you assign a different function value to `Error.prepareStackTrace` that function is used to format stack traces. It gets passed the error object that it is preparing a stack trace for, as well as a structured representation of the stack. User stack trace formatters are free to format the stack trace however they want and even return non-string values. It is safe to retain references to the structured stack trace object after a call to `prepareStackTrace` completes so that it is also a valid return value. Note that the custom `prepareStackTrace` function is only called once the stack property of `Error` object is accessed.

The structured stack trace is an array of `CallSite` objects, each of which represents a stack frame. A `CallSite` object defines the following methods

- `getThis`: returns the value of `this`
- `getTypeName`: returns the type of `this` as a string.  This is the name of the function stored in the constructor field of `this`, if available, otherwise the object’s `[[Class]]` internal property.
- `getFunction`: returns the current function
- `getFunctionName`: returns the name of the current function, typically its `name` property.  If a `name` property is not available an attempt is made to infer a name from the function’s context.
- `getMethodName`: returns the name of the property of `this` or one of its prototypes that holds the current function
- `getFileName`: if this function was defined in a script returns the name of the script
- `getLineNumber`: if this function was defined in a script returns the current line number
- `getColumnNumber`: if this function was defined in a script returns the current column number
- `getEvalOrigin`: if this function was created using a call to `eval` returns a string representing the location where `eval` was called
- `isToplevel`: is this a top-level invocation, that is, is this the global object?
- `isEval`: does this call take place in code defined by a call to `eval`?
- `isNative`: is this call in native V8 code?
- `isConstructor`: is this a constructor call?
- `isAsync`: is this an async call (i.e. `await`, `Promise.all()`, or `Promise.any()`)?
- `isPromiseAll`: is this an async call to `Promise.all()`?
- `getPromiseIndex`: returns the index of the promise element that was followed in `Promise.all()` or `Promise.any()` for async stack traces, or `null` if the `CallSite` is not an async `Promise.all()` or `Promise.any()` call.

The default stack trace is created using the CallSite API so any information that is available there is also available through this API.

To maintain restrictions imposed on strict mode functions, frames that have a strict mode function and all frames below (its caller etc.) are not allow to access their receiver and function objects. For those frames, `getFunction()` and `getThis()` returns `undefined`.

## Compatibility

The API described here is specific to V8 and is not supported by any other JavaScript implementations. Most implementations do provide an `error.stack` property but the format of the stack trace is likely to be different from the format described here. The recommended use of this API is:

- Only rely on the layout of the formatted stack trace if you know your code is running in v8.
- It is safe to set `Error.stackTraceLimit` and `Error.prepareStackTrace` regardless of which implementation is running your code but be aware that it only has an effect if your code is running in V8.

## Appendix: Stack trace format

The default stack trace format used by V8 can for each stack frame give the following information:

- Whether the call is a construct call.
- The type of the `this` value (`Type`).
- The name of the function called (`functionName`).
- The name of the property of this or one of its prototypes that holds the function (`methodName`).
- The current location within the source (`location`)

Any of these may be unavailable and different formats for stack frames are used depending on how much of this information is available.  If all the above information is available a formatted stack frame looks like this:

```
at Type.functionName [as methodName] (location)
```

Or, in the case of a construct call:

```
at new functionName (location)
```

Or, in case of an async call:

```
at async functionName (location)
```

If only one of `functionName` and `methodName` is available, or if they are both available but the same, the format is:

```
at Type.name (location)
```

If neither is available `<anonymous>` is used as the name.

The `Type` value is the name of the function stored in the constructor field of `this`. In V8, all constructor calls set this property to the constructor function so unless this field has been actively changed after the object was created it, it holds the name of the function it was created by. If it is unavailable the `[[Class]]` property of the object is used.

One special case is the global object where the `Type` is not shown. In that case the stack frame is formatted as:

```
at functionName [as methodName] (location)
```

The location itself has several possible formats. Most common is the file name, line and column number within the script that defined the current function:

```
fileName:lineNumber:columnNumber
```

If the current function was created using `eval` the format is:

```
eval at position
```

…where `position` is the full position where the call to `eval` occurred. Note that this means that positions can be nested if there are nested calls to `eval`, for instance:

```
eval at Foo.a (eval at Bar.z (myscript.js:10:3))
```

If a stack frame is within V8’s libraries the location is:

```
native
```

…and if is unavailable, it’s:

```
unknown location
```
