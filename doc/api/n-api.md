# N-API

> Stability: 1 - Experimental

N-API (pronounced N as in the letter, followed by API)
is an API for building native Addons. It is independent from
the underlying JavaScript runtime (ex V8) and is maintained as part of
Node.js itself. This API will be Application Binary Interface (ABI) stable
across versions of Node.js. It is intended to insulate Addons from
changes in the underlying JavaScript engine and allow modules
compiled for one version to run on later versions of Node.js without
recompilation.

Addons are built/packaged with the same approach/tools
outlined in the section titled  [C++ Addons](addons.html).
The only difference is the set of APIs that are used by the native code.
Instead of using the V8 or [Native Abstractions for Node.js][] APIs,
the functions available in the N-API are used.

APIs exposed by N-API are generally used to create and manipulate
JavaScript values. Concepts and operations generally map to ideas specified
in the ECMA262 Language Specification. The APIs have the following
properties:
- All N-API calls return a status code of type `napi_status`. This
  status indicates whether the API call succeeded or failed.
- The API's return value is passed via an out parameter.
- All JavaScript values are abstracted behind an opaque type named
  `napi_value`.
- In case of an error status code, additional information can be obtained
  using `napi_get_last_error_info`. More information can be found in the error
  handling section [Error Handling][].

The documentation for N-API is structured as follows:

* [Basic N-API Data Types][]
* [Error Handling][]
* [Object Lifetime Management][]
* [Module Registration][]
* [Working with JavaScript Values][]
* [Working with JavaScript Values - Abstract Operations][]
* [Working with JavaScript Properties][]
* [Working with JavaScript Functions][]
* [Object Wrap][]
* [Asynchronous Operations][]
* [Promises][]

The N-API is a C API that ensures ABI stability across Node.js versions
and different compiler levels. However, we also understand that a C++
API can be easier to use in many cases. To support these cases we expect
there to be one or more C++ wrapper modules that provide an inlineable C++
API. Binaries built with these wrapper modules will depend on the symbols
for the N-API C based functions exported by Node.js. These wrappers are not
part of N-API, nor will they be maintained as part of Node.js. One such
example is: [node-api](https://github.com/nodejs/node-api).

In order to use the N-API functions, include the file
[node_api.h](https://github.com/nodejs/node/blob/master/src/node_api.h)
which is located in the src directory in the node development tree.
For example:
```C
#include <node_api.h>
```

As the feature is experimental it must be enabled with the
following command line
[option](https://nodejs.org/dist/latest-v8.x/docs/api/cli.html#cli_napi_modules):

```bash
--napi-modules
```

## Basic N-API Data Types

N-API exposes the following fundamental datatypes as abstractions that are
consumed by the various APIs. These APIs should be treated as opaque,
introspectable only with other N-API calls.

### *napi_status*
Integral status code indicating the success or failure of a N-API call.
Currently, the following status codes are supported.
```C
typedef enum {
  napi_ok,
  napi_invalid_arg,
  napi_object_expected,
  napi_string_expected,
  napi_name_expected,
  napi_function_expected,
  napi_number_expected,
  napi_boolean_expected,
  napi_array_expected,
  napi_generic_failure,
  napi_pending_exception,
  napi_cancelled,
  napi_status_last
} napi_status;
```
If additional information is required upon an API returning a failed status,
it can be obtained by calling `napi_get_last_error_info`.

### *napi_extended_error_info*
```C
typedef struct {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
} napi_extended_error_info;
```

- `error_message`: UTF8-encoded string containing a VM-neutral description of
  the error.
- `engine_reserved`: Reserved for VM-specific error details. This is currently
  not implemented for any VM.
- `engine_error_code`: VM-specific error code. This is currently
  not implemented for any VM.
- `error_code`: The N-API status code that originated with the last error.

See the [Error Handling][] section for additional information.

### *napi_env*
`napi_env` is used to represent a context that the underlying N-API
implementation can use to persist VM-specific state. This structure is passed
to native functions when they're invoked, and it must be passed back when
making N-API calls. Specifically, the same `napi_env` that was passed in when
the initial native function was called must be passed to any subsequent
nested N-API calls. Caching the `napi_env` for the purpose of general reuse is
not allowed.

### *napi_value*
This is an opaque pointer that is used to represent a JavaScript value.

### N-API Memory Management types
#### *napi_handle_scope*
This is an abstraction used to control and modify the lifetime of objects
created within a particular scope. In general, N-API values are created within
the context of a handle scope. When a native method is called from
JavaScript, a default handle scope will exist. If the user does not explicitly
create a new handle scope, N-API values will be created in the default handle
scope. For any invocations of code outside the execution of a native method
(for instance, during a libuv callback invocation), the module is required to
create a scope before invoking any functions that can result in the creation
of JavaScript values.

Handle scopes are created using [`napi_open_handle_scope`][] and are destroyed
using [`napi_close_handle_scope`][]. Closing the scope can indicate to the GC that
all `napi_value`s created during the lifetime of the handle scope are no longer
referenced from the current stack frame.

For more details, review the [Object Lifetime Management][].

#### *napi_escapable_handle_scope*
Escapable handle scopes are a special type of handle scope to return values
created within a particular handle scope to a parent scope.

#### *napi_ref*
This is the abstraction to use to reference a `napi_value`. This allows for
users to manage the lifetimes of JavaScript values, including defining their
minimum lifetimes explicitly.

For more details, review the [Object Lifetime Management][].

### N-API Callback types
#### *napi_callback_info*
Opaque datatype that is passed to a callback function. It can be used for
getting additional information about the context in which the callback was
invoked.

#### *napi_callback*
Function pointer type for user-provided native functions which are to be
exposed to JavaScript via N-API. Callback functions should satisfy the
following signature:
```C
typedef napi_value (*napi_callback)(napi_env, napi_callback_info);
```

#### *napi_finalize*
Function pointer type for add-on provided functions that allow the user to be
notified when externally-owned data is ready to be cleaned up because the
object with which it was associated with, has been garbage-collected. The user
must provide a function satisfying the following signature which would get
called upon the object's collection. Currently, `napi_finalize` can be used for
finding out when objects that have external data are collected.

```C
typedef void (*napi_finalize)(napi_env env,
                              void* finalize_data,
                              void* finalize_hint);
```


#### napi_async_execute_callback
Function pointer used with functions that support asynchronous
operations. Callback functions must statisfy the following signature:

```C
typedef void (*napi_async_execute_callback)(napi_env env, void* data);
```

#### napi_async_complete_callback
Function pointer used with functions that support asynchronous
operations. Callback functions must statisfy the following signature:

```C
typedef void (*napi_async_complete_callback)(napi_env env,
                                             napi_status status,
                                             void* data);
```

## Error Handling
N-API uses both return values and Javascript exceptions for error handling.
The following sections explain the approach for each case.

### Return values
All of the N-API functions share the same error handling pattern. The
return type of all API functions is `napi_status`.

The return value will be `napi_ok` if the request was successful and
no uncaught JavaScript exception was thrown. If an error occurred AND
an exception was thrown, the `napi_status` value for the error
will be returned. If an exception was thrown, and no error occurred,
`napi_pending_exception` will be returned.

In cases where a return value other than `napi_ok` or
`napi_pending_exception` is returned, [`napi_is_exception_pending`][]
must be called to check if an exception is pending.
See the section on exceptions for more details.

The full set of possible napi_status values is defined
in `napi_api_types.h`.

The `napi_status` return value provides a VM-independent representation of
the error which occurred. In some cases it is useful to be able to get
more detailed information, including a string representing the error as well as
VM (engine)-specific information.

In order to retrieve this information [`napi_get_last_error_info`][]
is provided which returns a `napi_extended_error_info` structure.
The format of the `napi_extended_error_info` structure is as follows:

```C
typedef struct napi_extended_error_info {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
};
```
- `error_message`: Textual representation of the error that occurred.
- `engine_reserved`: Opaque handle reserved for engine use only.
- `engine_error_code`: VM specific error code.
- `error_code`: n-api status code for the last error.

[`napi_get_last_error_info`][] returns the information for the last
N-API call that was made.

*Note*: Do not rely on the content or format of any of the extended
information as it is not subject to SemVer and may change at any time.
It is intended only for logging purposes.

#### napi_get_last_error_info
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status
napi_get_last_error_info(napi_env env,
                         const napi_extended_error_info** result);
```
- `[in] env`: The environment that the API is invoked under.
- `[out] result`: The `napi_extended_error_info` structure with more
information about the error.

Returns `napi_ok` if the API succeeded.

This API retrieves a `napi_extended_error_info` structure with information
about the last error that occurred.

*Note*: The content of the `napi_extended_error_info` returned is only
valid up until an n-api function is called on the same `env`.

*Note*: Do not rely on the content or format of any of the extended
information as it is not subject to SemVer and may change at any time.
It is intended only for logging purposes.


### Exceptions
Any N-API function call may result in a pending JavaScript exception. This is
obviously the case for any function that may cause the execution of
JavaScript, but N-API specifies that an exception may be pending
on return from any of the API functions.

If the `napi_status` returned by a function is `napi_ok` then no
exception is pending and no additional action is required. If the
`napi_status` returned is anything other than `napi_ok` or
`napi_pending_exception`, in order to try to recover and continue
instead of simply returning immediately, [`napi_is_exception_pending`][]
must be called in order to determine if an exception is pending or not.

When an exception is pending one of two approaches can be employed.

The first appoach is to do any appropriate cleanup and then return so that
execution will return to JavaScript. As part of the transition back to
JavaScript the exception will be thrown at the point in the JavaScript
code where the native method was invoked. The behavior of most N-API calls
is unspecified while an exception is pending, and many will simply return
`napi_pending_exception`, so it is important to do as little as possible
and then return to JavaScript where the exception can be handled.

The second approach is to try to handle the exception. There will be cases
where the native code can catch the exception, take the appropriate action,
and then continue. This is only recommended in specific cases
where it is known that the exception can be safely handled. In these
cases [`napi_get_and_clear_last_exception`][] can be used to get and
clear the exception.  On success, result will contain the handle to
the last JavaScript Object thrown. If it is determined, after
retrieving the exception, the exception cannot be handled after all
it can be re-thrown it with [`napi_throw`][] where error is the
JavaScript Error object to be thrown.

The following utility functions are also available in case native code
needs to throw an exception or determine if a `napi_value` is an instance
of a JavaScript `Error` object:  [`napi_throw_error`][],
[`napi_throw_type_error`][], [`napi_throw_range_error`][] and
[`napi_is_error`][].

The following utility functions are also available in case native
code needs to create an Error object: [`napi_create_error`][],
[`napi_create_type_error`][], and [`napi_create_range_error`][].
where result is the napi_value that refers to the newly created
JavaScript Error object.

The Node.js project is adding error codes to all of the errors
generated internally.  The goal is for applications to use these
error codes for all error checking. The associated error messages
will remain, but will only be meant to be used for logging and
display with the expectation that the message can change without
SemVer applying. In order to support this model with N-API, both
in internal functionality and for module specific functionality
(as its good practice), the `throw_` and `create_` functions
take an optional code parameter which is the string for the code
to be added to the error object.  If the optional parameter is NULL
then no code will be associated with the error. If a code is provided,
the name associated with the error is also updated to be:

```text
originalName [code]
```

where originalName is the original name associated with the error
and code is the code that was provided.  For example if the code
is 'ERR_ERROR_1' and a TypeError is being created the name will be:

```text
TypeError [ERR_ERROR_1]
```

#### napi_throw
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_throw(napi_env env, napi_value error);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] error`: The `napi_value` for the Error to be thrown.

Returns `napi_ok` if the API succeeded.

This API throws the JavaScript Error provided.


#### napi_throw_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_throw_error(napi_env env,
                                         const char* code,
                                         const char* msg);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] code`: Optional error code to be set on the error.
- `[in] msg`: C string representing the text to be associated with
the error.

Returns `napi_ok` if the API succeeded.

This API throws a JavaScript Error with the text provided.

#### napi_throw_type_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_throw_type_error(napi_env env,
                                              const char* code,
                                              const char* msg);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] code`: Optional error code to be set on the error.
- `[in] msg`: C string representing the text to be associated with
the error.

Returns `napi_ok` if the API succeeded.

This API throws a JavaScript TypeError with the text provided.

#### napi_throw_range_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_throw_range_error(napi_env env,
                                               const char* code,
                                               const char* msg);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] code`: Optional error code to be set on the error.
- `[in] msg`: C string representing the text to be associated with
the error.

Returns `napi_ok` if the API succeeded.

This API throws a JavaScript RangeError with the text provided.


#### napi_is_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_is_error(napi_env env,
                                      napi_value value,
                                      bool* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] msg`: The `napi_value` to be checked.
- `[out] result`: Boolean value that is set to true if `napi_value` represents
an error, false otherwise.

Returns `napi_ok` if the API succeeded.

This API queries a `napi_value` to check if it represents an error object.


#### napi_create_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_create_error(napi_env env,
                                          napi_value code,
                                          napi_value msg,
                                          napi_value* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] code`: Optional `napi_value` with the string for the error code to
               be associated with the error.
- `[in] msg`: napi_value that references a JavaScript String to be
used as the message for the Error.
- `[out] result`: `napi_value` representing the error created.

Returns `napi_ok` if the API succeeded.

This API returns a JavaScript Error with the text provided.

#### napi_create_type_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_create_type_error(napi_env env,
                                               napi_value code,
                                               napi_value msg,
                                               napi_value* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] code`: Optional `napi_value` with the string for the error code to
               be associated with the error.
- `[in] msg`: napi_value that references a JavaScript String to be
used as the message for the Error.
- `[out] result`: `napi_value` representing the error created.

Returns `napi_ok` if the API succeeded.

This API returns a JavaScript TypeError with the text provided.


#### napi_create_range_error
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_create_range_error(napi_env env,
                                                napi_value code,
                                                const char* msg,
                                                napi_value* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] code`: Optional `napi_value` with the string for the error code to
               be associated with the error.
- `[in] msg`: napi_value that references a JavaScript String to be
used as the message for the Error.
- `[out] result`: `napi_value` representing the error created.

Returns `napi_ok` if the API succeeded.

This API returns a JavaScript RangeError with the text provided.


#### napi_get_and_clear_last_exception
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_get_and_clear_last_exception(napi_env env,
                                                          napi_value* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: The exception if one is pending, NULL otherwise.

Returns `napi_ok` if the API succeeded.

This API returns true if an exception is pending.

#### napi_is_exception_pending
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_is_exception_pending(napi_env env, bool* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: Boolean value that is set to true if an exception is pending.

Returns `napi_ok` if the API succeeded.

This API returns true if an exception is pending.

### Fatal Errors

In the event of an unrecoverable error in a native module, a fatal error can be
thrown to immediately terminate the process.

#### napi_fatal_error
<!-- YAML
added: v8.2.0
-->
```C
NAPI_EXTERN NAPI_NO_RETURN void napi_fatal_error(const char* location, const char* message);
```

- `[in] location`: Optional location at which the error occurred.
- `[in] message`: The message associated with the error.

The function call does not return, the process will be terminated.

## Object Lifetime management

As N-API calls are made, handles to objects in the heap for the underlying
VM may be returned as `napi_values`. These handles must hold the
objects 'live' until they are no longer required by the native code,
otherwise the objects could be collected before the native code was
finished using them.

As object handles are returned they are associated with a
'scope'. The lifespan for the default scope is tied to the lifespan
of the native method call. The result is that, by default, handles
remain valid and the objects associated with these handles will be
held live for the lifespan of the native method call.

In many cases, however, it is necessary that the handles remain valid for
either a shorter or longer lifespan than that of the native method.
The sections which follow describe the N-API functions than can be used
to change the handle lifespan from the default.

### Making handle lifespan shorter than that of the native method
It is often necessary to make the lifespan of handles shorter than
the lifespan of a native method. For example, consider a native method
that has a loop which iterates through the elements in a large array:

```C
for (int i = 0; i < 1000000; i++) {
  napi_value result;
  napi_status status = napi_get_element(e object, i, &result);
  if (status != napi_ok) {
    break;
  }
  // do something with element
}
```

This would result in a large number of handles being created, consuming
substantial resources. In addition, even though the native code could only
use the most recent handle, all of the associated objects would also be
kept alive since they all share the same scope.

To handle this case, N-API provides the ability to establish a new 'scope' to
which newly created handles will be associated. Once those handles
are no longer required, the scope can be 'closed' and any handles associated
with the scope are invalidated. The methods available to open/close scopes are
[`napi_open_handle_scope`][] and [`napi_close_handle_scope`][].

N-API only supports a single nested hiearchy of scopes. There is only one
active scope at any time, and all new handles will be associated with that
scope while it is active. Scopes must be closed in the reverse order from
which they are opened. In addition, all scopes created within a native method
must be closed before returning from that method.

Taking the earlier example, adding calls to [`napi_open_handle_scope`][] and
[`napi_close_handle_scope`][] would ensure that at most a single handle
is valid throughout the execution of the loop:

```C
for (int i = 0; i < 1000000; i++) {
  napi_handle_scope scope;
  napi_status status = napi_open_handle_scope(env, &scope);
  if (status != napi_ok) {
    break;
  }
  napi_value result;
  status = napi_get_element(e object, i, &result);
  if (status != napi_ok) {
    break;
  }
  // do something with element
  status = napi_close_handle_scope(env, scope);
  if (status != napi_ok) {
    break;
  }
}
```

When nesting scopes, there are cases where a handle from an
inner scope needs to live beyond the lifespan of that scope. N-API supports an
'escapable scope' in order to support this case. An escapable scope
allows one handle to be 'promoted' so that it 'escapes' the
current scope and the lifespan of the handle changes from the current
scope to that of the outer scope.

The methods available to open/close escapable scopes are
[`napi_open_escapable_handle_scope`][] and [`napi_close_escapable_handle_scope`][].

The request to promote a handle is made through [`napi_escape_handle`][] which
can only be called once.

#### napi_open_handle_scope
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_open_handle_scope(napi_env env,
                                               napi_handle_scope* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[out] result`: `napi_value` representing the new scope.

Returns `napi_ok` if the API succeeded.

This API open a new scope.

#### napi_close_handle_scope
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_close_handle_scope(napi_env env,
                                                napi_handle_scope scope);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] scope`: `napi_value` representing the scope to be closed.

Returns `napi_ok` if the API succeeded.

This API closes the scope passed in. Scopes must be closed in the
reverse order from which they were created.

#### napi_open_escapable_handle_scope
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status
    napi_open_escapable_handle_scope(napi_env env,
                                     napi_handle_scope* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[out] result`: `napi_value` representing the new scope.

Returns `napi_ok` if the API succeeded.

This API open a new scope from which one object can be promoted
to the outer scope.

#### napi_close_escapable_handle_scope
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status
    napi_close_escapable_handle_scope(napi_env env,
                                      napi_handle_scope scope);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] scope`: `napi_value` representing the scope to be closed.

Returns `napi_ok` if the API succeeded.

This API closes the scope passed in. Scopes must be closed in the
reverse order from which they were created.

#### napi_escape_handle
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_escape_handle(napi_env env,
                                           napi_escapable_handle_scope scope,
                                           napi_value escapee,
                                           napi_value* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] scope`: `napi_value` representing the current scope.
- `[in] escapee`: `napi_value` representing the JavaScript Object to be escaped.
- `[out] result`: `napi_value` representing the handle to the escaped
Object in the outer scope.

Returns `napi_ok` if the API succeeded.

This API promotes the handle to the JavaScript object so that it is valid
for the lifetime of the outer scope. It can only be called once per scope.
If it is called more than once an error will be returned.

### References to objects with a lifespan longer than that of the native method
In some cases an addon will need to be able to create and reference objects
with a lifespan longer than that of a single native method invocation. For
example, to create a constructor and later use that constructor
in a request to creates instances, it must be possible to reference
the constructor object across many different instance creation requests. This
would not be possible with a normal handle returned as a `napi_value` as
described in the earlier section. The lifespan of a normal handle is
managed by scopes and all scopes must be closed before the end of a native
method.

N-API provides methods to create persistent references to an object.
Each persistent reference has an associated count with a value of 0
or higher. The count determines if the reference will keep
the corresponding object live. References with a count of 0 do not
prevent the object from being collected and are often called 'weak'
references. Any count greater than 0 will prevent the object
from being collected.

References can be created with an initial reference count. The count can
then be modified through [`napi_reference_ref`][] and
[`napi_reference_unref`][]. If an object is collected while the count
for a reference is 0, all subsequent calls to
get the object associated with the reference [`napi_get_reference_value`][]
will return NULL for the returned `napi_value`. An attempt to call
[`napi_reference_ref`][] for a reference whose object has been collected
will result in an error.

References must be deleted once they are no longer required by the addon. When
a reference is deleted it will no longer prevent the corresponding object from
being collected. Failure to delete a persistent reference will result in
a 'memory leak' with both the native memory for the persistent reference and
the corresponding object on the heap being retained forever.

There can be multiple persistent references created which refer to the same
object, each of which will either keep the object live or not based on its
individual count.

#### napi_create_reference
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_create_reference(napi_env env,
                                              napi_value value,
                                              int initial_refcount,
                                              napi_ref* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing the Object to which we want
a reference to.
- `[in] initial_refcount`: Initial reference count for the new reference.
- `[out] result`: `napi_ref` pointing to the new reference.

Returns `napi_ok` if the API succeeded.

This API create a new reference with the specified reference count
to the Object passed in.

#### napi_delete_reference
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_delete_reference(napi_env env, napi_ref ref);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] ref`: `napi_ref` to be deleted.

Returns `napi_ok` if the API succeeded.

This API deletes the reference passed in.

#### napi_reference_ref
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_reference_ref(napi_env env,
                                           napi_ref ref,
                                           int* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] ref`: `napi_ref` for which the reference count will be incremented.
- `[out] result`: The new reference count.

Returns `napi_ok` if the API succeeded.

This API increments the reference count for the reference
passed in and returns the resulting reference count.


#### napi_reference_unref
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_reference_unref(napi_env env,
                                             napi_ref ref,
                                             int* result);
```
- `[in] env`: The environment that the API is invoked under.
- `[in] ref`: `napi_ref` for which the reference count will be decremented.
- `[out] result`: The new reference count.

Returns `napi_ok` if the API succeeded.

This API decrements the reference count for the reference
passed in and returns the resulting reference count.


#### napi_get_reference_value
<!-- YAML
added: v8.0.0
-->
```C
NODE_EXTERN napi_status napi_get_reference_value(napi_env env,
                                                 napi_ref ref,
                                                 napi_value* result);
```

the `napi_value passed` in or out of these methods is a handle to the
object to which the reference is related.
- `[in] env`: The environment that the API is invoked under.
- `[in] ref`: `napi_ref` for which we requesting the corresponding Object.
- `[out] result`: The `napi_value` for the Object referenced by the
`napi_ref`.

Returns `napi_ok` if the API succeeded.

If still valid, this API returns the `napi_value` representing the
JavaScript Object associated with the `napi_ref`. Otherise, result
will be NULL.

## Module registration
N-API modules are registered in the same manner as other modules
except that instead of using the `NODE_MODULE` macro the following
is used:

```C
NAPI_MODULE(addon, Init)
```

The next difference is the signature for the `Init` method. For a N-API
module it is as follows:

```C
void Init(napi_env env, napi_value exports, napi_value module, void* priv);
```

As with any other module, functions are exported by either adding them to
the `exports` or `module` objects passed to the `Init` method.

For example, to add the method `hello` as a function so that it can be called
as a method provided by the addon:

```C
void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc =
    {"hello", Method, 0, 0, 0, napi_default, 0};
  status = napi_define_properties(env, exports, 1, &desc);
}
```

For example, to set a function to be returned by the `require()` for the addon:

```C
void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc =
    {"exports", Method, 0, 0, 0, napi_default, 0};
  status = napi_define_properties(env, module, 1, &desc);
}
```

For example, to define a class so that new instances can be created
(often used with [Object Wrap][]):

```C
// NOTE: partial example, not all referenced code is included

napi_status status;
napi_property_descriptor properties[] = {
    { "value", nullptr, GetValue, SetValue, 0, napi_default, 0 },
    DECLARE_NAPI_METHOD("plusOne", PlusOne),
    DECLARE_NAPI_METHOD("multiply", Multiply),
};

napi_value cons;
status =
    napi_define_class(env, "MyObject", New, nullptr, 3, properties, &cons);
if (status != napi_ok) return;

status = napi_create_reference(env, cons, 1, &constructor);
if (status != napi_ok) return;

status = napi_set_named_property(env, exports, "MyObject", cons);
if (status != napi_ok) return;
```

For more details on setting properties on either the `exports` or `module`
objects, see the section on
[Working with JavaScript Properties][].

For more details on building addon modules in general, refer to the existing API

## Working with JavaScript Values
N-API exposes a set of APIs to create all types of JavaScript values.
Some of these types are documented under
[Section 6](https://tc39.github.io/ecma262/#sec-ecmascript-data-types-and-values)
of the [ECMAScript Language Specification][].

Fundamentally, these APIs are used to do one of the following:
1. Create a new JavaScript object
2. Convert from a primitive C type to an N-API value
3. Convert from N-API value to a primitive C type
4. Get global instances including `undefined` and `null`

N-API values are represented by the type `napi_value`.
Any N-API call that requires a JavaScript value takes in a `napi_value`.
In some cases, the API does check the type of the `napi_value` up-front.
However, for better performance, it's better for the caller to make sure that
the `napi_value` in question is of the JavaScript type expected by the API.

### Enum types
#### *napi_valuetype*
```C
typedef enum {
  // ES6 types (corresponds to typeof)
  napi_undefined,
  napi_null,
  napi_boolean,
  napi_number,
  napi_string,
  napi_symbol,
  napi_object,
  napi_function,
  napi_external,
} napi_valuetype;
```

Describes the type of a `napi_value`. This generally corresponds to the types
described in
[Section 6.1](https://tc39.github.io/ecma262/#sec-ecmascript-language-types) of
the ECMAScript Language Specification.
In addition to types in that section, `napi_valuetype` can also represent
Functions and Objects with external data.

#### *napi_typedarray_type*
```C
typedef enum {
  napi_int8_array,
  napi_uint8_array,
  napi_uint8_clamped_array,
  napi_int16_array,
  napi_uint16_array,
  napi_int32_array,
  napi_uint32_array,
  napi_float32_array,
  napi_float64_array,
} napi_typedarray_type;
```

This represents the underlying binary scalar datatype of the TypedArray.
Elements of this enum correspond to
[Section 22.2](https://tc39.github.io/ecma262/#sec-typedarray-objects)
of the [ECMAScript Language Specification][].

### Object Creation Functions
#### *napi_create_array*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_array(napi_env env, napi_value* result)
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[out] result`: A `napi_value` representing a JavaScript Array.

Returns `napi_ok` if the API succeeded.

This API returns an N-API value corresponding to a JavaScript Array type.
JavaScript arrays are described in
[Section 22.1](https://tc39.github.io/ecma262/#sec-array-objects) of the
ECMAScript Language Specification.

#### *napi_create_array_with_length*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_array_with_length(napi_env env,
                                          size_t length,
                                          napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] length`: The initial length of the Array.
- `[out] result`: A `napi_value` representing a JavaScript Array.

Returns `napi_ok` if the API succeeded.

This API returns an N-API value corresponding to a JavaScript Array type.
The Array's length property is set to the passed-in length parameter.
However, the underlying buffer is not guaranteed to be pre-allocated by the VM
when the array is created - that behavior is left to the underlying VM
implementation.
If the buffer must be a contiguous block of memory that can be
directly read and/or written via C, consider using
[`napi_create_external_arraybuffer`][].

JavaScript arrays are described in
[Section 22.1](https://tc39.github.io/ecma262/#sec-array-objects) of the
ECMAScript Language Specification.

#### *napi_create_arraybuffer*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_arraybuffer(napi_env env,
                                    size_t byte_length,
                                    void** data,
                                    napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] length`: The length in bytes of the array buffer to create.
- `[out] data`: Pointer to the underlying byte buffer of the ArrayBuffer.
- `[out] result`: A `napi_value` representing a JavaScript ArrayBuffer.

Returns `napi_ok` if the API succeeded.

This API returns an N-API value corresponding to a JavaScript ArrayBuffer.
ArrayBuffers are used to represent fixed-length binary data buffers. They are
normally used as a backing-buffer for TypedArray objects.
The ArrayBuffer allocated will have an underlying byte buffer whose size is
determined by the `length` parameter that's passed in.
The underlying buffer is optionally returned back to the caller in case the
caller wants to directly manipulate the buffer. This buffer can only be
written to directly from native code. To write to this buffer from JavaScript,
a typed array or DataView object would need to be created.

JavaScript ArrayBuffer objects are described in
[Section 24.1](https://tc39.github.io/ecma262/#sec-arraybuffer-objects)
of the ECMAScript Language Specification.

#### *napi_create_buffer*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_buffer(napi_env env,
                               size_t size,
                               void** data,
                               napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] size`: Size in bytes of the underlying buffer.
- `[out] data`: Raw pointer to the underlying buffer.
- `[out] result`: A `napi_value` representing a `node::Buffer`.

Returns `napi_ok` if the API succeeded.

This API allocates a `node::Buffer` object. While this is still a
fully-supported data structure, in most cases using a TypedArray will suffice.

#### *napi_create_buffer_copy*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_buffer_copy(napi_env env,
                                    size_t length,
                                    const void* data,
                                    void** result_data,
                                    napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] size`: Size in bytes of the input buffer (should be the same as the
 size of the new buffer).
- `[in] data`: Raw pointer to the underlying buffer to copy from.
- `[out] result_data`: Pointer to the new Buffer's underlying data buffer.
- `[out] result`: A `napi_value` representing a `node::Buffer`.

Returns `napi_ok` if the API succeeded.

This API allocates a `node::Buffer` object and initializes it with data copied
from the passed-in buffer. While this is still a fully-supported data
structure, in most cases using a TypedArray will suffice.

#### *napi_create_external*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_external(napi_env env,
                                 void* data,
                                 napi_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] data`: Raw pointer to the external data.
- `[in] finalize_cb`: Optional callback to call when the external value
is being collected.
- `[in] finalize_hint`: Optional hint to pass to the finalize callback
during collection.
- `[out] result`: A `napi_value` representing an external value.

Returns `napi_ok` if the API succeeded.

This API allocates a JavaScript value with external data attached to it. This
is used to pass external data through JavaScript code, so it can be retrieved
later by native code. The API allows the caller to pass in a finalize callback,
in case the underlying native resource needs to be cleaned up when the external
JavaScript value gets collected.

*Note*: The created value is not an object, and therefore does not support
additional properties. It is considered a distinct value type: calling
`napi_typeof()` with an external value yields `napi_external`.

#### napi_create_external_arraybuffer
<!-- YAML
added: v8.0.0
-->
```C
napi_status
napi_create_external_arraybuffer(napi_env env,
                                 void* external_data,
                                 size_t byte_length,
                                 napi_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] external_data`: Pointer to the underlying byte buffer of the
ArrayBuffer.
- `[in] byte_length`: The length in bytes of the underlying buffer.
- `[in] finalize_cb`: Optional callback to call when the ArrayBuffer is
being collected.
- `[in] finalize_hint`: Optional hint to pass to the finalize callback
during collection.
- `[out] result`: A `napi_value` representing a JavaScript ArrayBuffer.

Returns `napi_ok` if the API succeeded.

This API returns an N-API value corresponding to a JavaScript ArrayBuffer.
The underlying byte buffer of the ArrayBuffer is externally allocated and
managed. The caller must ensure that the byte buffer remains valid until the
finalize callback is called.

JavaScript ArrayBuffers are described in
[Section 24.1](https://tc39.github.io/ecma262/#sec-arraybuffer-objects)
of the ECMAScript Language Specification.

#### *napi_create_external_buffer*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_external_buffer(napi_env env,
                                        size_t length,
                                        void* data,
                                        napi_finalize finalize_cb,
                                        void* finalize_hint,
                                        napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] length`: Size in bytes of the input buffer (should be the same as
the size of the new buffer).
- `[in] data`: Raw pointer to the underlying buffer to copy from.
- `[in] finalize_cb`: Optional callback to call when the ArrayBuffer is
being collected.
- `[in] finalize_hint`: Optional hint to pass to the finalize callback
during collection.
- `[out] result`: A `napi_value` representing a `node::Buffer`.

Returns `napi_ok` if the API succeeded.

This API allocates a `node::Buffer` object and initializes it with data
backed by the passed in buffer. While this is still a fully-supported data
structure, in most cases using a TypedArray will suffice.

*Note*: For Node.js >=4 `Buffers` are Uint8Arrays.

#### *napi_create_function*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_function(napi_env env,
                                 const char* utf8name,
                                 napi_callback cb,
                                 void* data,
                                 napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] utf8name`: A string representing the name of the function encoded as
UTF8.
- `[in] cb`: A function pointer to the native function to be invoked when the
created function is invoked from JavaScript.
- `[in] data`: Optional arbitrary context data to be passed into the native
function when it is invoked.
- `[out] result`: A `napi_value` representing a JavaScript function.

Returns `napi_ok` if the API succeeded.

This API returns an N-API value corresponding to a JavaScript Function object.
It's used to wrap native functions so that they can be invoked from JavaScript.

JavaScript Functions are described in
[Section 19.2](https://tc39.github.io/ecma262/#sec-function-objects)
of the ECMAScript Language Specification.

#### *napi_create_object*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_object(napi_env env, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: A `napi_value` representing a JavaScript Object.

Returns `napi_ok` if the API succeeded.

This API allocates a default JavaScript Object.
It is the equivalent of doing `new Object()` in JavaScript.

The JavaScript Object type is described in
[Section 6.1.7](https://tc39.github.io/ecma262/#sec-object-type) of the
ECMAScript Language Specification.

#### *napi_create_symbol*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_symbol(napi_env env,
                               napi_value description,
                               napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] description`: Optional napi_value which refers to a JavaScript
String to be set as the description for the symbol.
- `[out] result`: A `napi_value` representing a JavaScript Symbol.

Returns `napi_ok` if the API succeeded.

This API creates a JavaScript Symbol object from a UTF8-encoded C string

The JavaScript Symbol type is described in
[Section 19.4](https://tc39.github.io/ecma262/#sec-symbol-objects)
of the ECMAScript Language Specification.

#### *napi_create_typedarray*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_typedarray(napi_env env,
                                   napi_typedarray_type type,
                                   size_t length,
                                   napi_value arraybuffer,
                                   size_t byte_offset,
                                   napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] type`: Scalar datatype of the elements within the TypedArray.
- `[in] length`: Number of elements in the TypedArray.
- `[in] arraybuffer`: ArrayBuffer underlying the typed array.
- `[in] byte_offset`: The byte offset within the ArrayBuffer from which to
start projecting the TypedArray.
- `[out] result`: A `napi_value` representing a JavaScript TypedArray.

Returns `napi_ok` if the API succeeded.

This API creates a JavaScript TypedArray object over an existing ArrayBuffer.
TypedArray objects provide an array-like view over an underlying data buffer
where each element has the same underlying binary scalar datatype.

It's required that (length * size_of_element) + byte_offset should
be <= the size in bytes of the array passed in. If not, a RangeError exception is
raised.

JavaScript TypedArray Objects are described in
[Section 22.2](https://tc39.github.io/ecma262/#sec-typedarray-objects)
of the ECMAScript Language Specification.


#### *napi_create_dataview*
<!-- YAML
added: v8.3.0
-->

```C
napi_status napi_create_dataview(napi_env env,
                                 size_t byte_length,
                                 napi_value arraybuffer,
                                 size_t byte_offset,
                                 napi_value* result)

```

- `[in] env`: The environment that the API is invoked under.
- `[in] length`: Number of elements in the DataView.
- `[in] arraybuffer`: ArrayBuffer underlying the DataView.
- `[in] byte_offset`: The byte offset within the ArrayBuffer from which to
  start projecting the DataView.
- `[out] result`: A `napi_value` representing a JavaScript DataView.

Returns `napi_ok` if the API succeeded.

This API creates a JavaScript DataView object over an existing ArrayBuffer.
DataView objects provide an array-like view over an underlying data buffer,
but one which allows items of different size and type in the ArrayBuffer.

It is required that `byte_length + byte_offset` is less than or equal to the
size in bytes of the array passed in. If not, a RangeError exception is raised.

JavaScript DataView Objects are described in
[Section 24.3][] of the ECMAScript Language Specification.

### Functions to convert from C types to N-API
#### *napi_create_int32*
<!-- YAML
added: v8.4.0
-->
```C
napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: Integer value to be represented in JavaScript.
- `[out] result`: A `napi_value` representing a JavaScript Number.

Returns `napi_ok` if the API succeeded.

This API is used to convert from the C `int32_t` type to the JavaScript
Number type.

The JavaScript Number type is described in
[Section 6.1.6](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-number-type)
of the ECMAScript Language Specification.

#### *napi_create_uint32*
<!-- YAML
added: v8.4.0
-->
```C
napi_status napi_create_uint32(napi_env env, uint32_t value, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: Unsigned integer value to be represented in JavaScript.
- `[out] result`: A `napi_value` representing a JavaScript Number.

Returns `napi_ok` if the API succeeded.

This API is used to convert from the C `uint32_t` type to the JavaScript
Number type.

The JavaScript Number type is described in
[Section 6.1.6](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-number-type)
of the ECMAScript Language Specification.

#### *napi_create_int64*
<!-- YAML
added: v8.4.0
-->
```C
napi_status napi_create_int64(napi_env env, int64_t value, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: Integer value to be represented in JavaScript.
- `[out] result`: A `napi_value` representing a JavaScript Number.

Returns `napi_ok` if the API succeeded.

This API is used to convert from the C `int64_t` type to the JavaScript
Number type.

The JavaScript Number type is described in
[Section 6.1.6](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-number-type)
of the ECMAScript Language Specification. Note the complete range of `int64_t`
cannot be represented with full precision in JavaScript. Integer values
outside the range of
[`Number.MIN_SAFE_INTEGER`](https://tc39.github.io/ecma262/#sec-number.min_safe_integer)
-(2^53 - 1) -
[`Number.MAX_SAFE_INTEGER`](https://tc39.github.io/ecma262/#sec-number.max_safe_integer)
(2^53 - 1) will lose precision.

#### *napi_create_double*
<!-- YAML
added: v8.4.0
-->
```C
napi_status napi_create_double(napi_env env, double value, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: Double-precision value to be represented in JavaScript.
- `[out] result`: A `napi_value` representing a JavaScript Number.

Returns `napi_ok` if the API succeeded.

This API is used to convert from the C `double` type to the JavaScript
Number type.

The JavaScript Number type is described in
[Section 6.1.6](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-number-type)
of the ECMAScript Language Specification.

#### *napi_create_string_latin1*
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_create_string_latin1(napi_env env,
                                                  const char* str,
                                                  size_t length,
                                                  napi_value* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] str`: Character buffer representing a ISO-8859-1-encoded string.
- `[in] length`: The length of the string in bytes, or -1 if it is
null-terminated.
- `[out] result`: A `napi_value` representing a JavaScript String.

Returns `napi_ok` if the API succeeded.

This API creates a JavaScript String object from a ISO-8859-1-encoded C string.

The JavaScript String type is described in
[Section 6.1.4](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-string-type)
of the ECMAScript Language Specification.

#### *napi_create_string_utf16*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_string_utf16(napi_env env,
                                     const char16_t* str,
                                     size_t length,
                                     napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] str`: Character buffer representing a UTF16-LE-encoded string.
- `[in] length`: The length of the string in two-byte code units, or -1 if
it is null-terminated.
- `[out] result`: A `napi_value` representing a JavaScript String.

Returns `napi_ok` if the API succeeded.

This API creates a JavaScript String object from a UTF16-LE-encoded C string

The JavaScript String type is described in
[Section 6.1.4](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-string-type)
of the ECMAScript Language Specification.

#### *napi_create_string_utf8*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_string_utf8(napi_env env,
                                    const char* str,
                                    size_t length,
                                    napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] str`: Character buffer representing a UTF8-encoded string.
- `[in] length`: The length of the string in bytes, or -1 if it is
null-terminated.
- `[out] result`: A `napi_value` representing a JavaScript String.

Returns `napi_ok` if the API succeeded.

This API creates a JavaScript String object from a UTF8-encoded C string

The JavaScript String type is described in
[Section 6.1.4](https://tc39.github.io/ecma262/#sec-ecmascript-language-types-string-type)
of the ECMAScript Language Specification.

### Functions to convert from N-API to C types
#### *napi_get_array_length*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_array_length(napi_env env,
                                  napi_value value,
                                  uint32_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing the JavaScript Array whose length is
being queried.
- `[out] result`: `uint32` representing length of the array.

Returns `napi_ok` if the API succeeded.

This API returns the length of an array.

Array length is described in
[Section 22.1.4.1](https://tc39.github.io/ecma262/#sec-properties-of-array-instances-length)
of the ECMAScript Language Specification.

#### *napi_get_arraybuffer_info*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_arraybuffer_info(napi_env env,
                                      napi_value arraybuffer,
                                      void** data,
                                      size_t* byte_length)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] arraybuffer`: `napi_value` representing the ArrayBuffer being queried.
- `[out] data`: The underlying data buffer of the ArrayBuffer.
- `[out] byte_length`: Length in bytes of the underlying data buffer.

Returns `napi_ok` if the API succeeded.

This API is used to retrieve the underlying data buffer of an ArrayBuffer and
its length.

*WARNING*: Use caution while using this API. The lifetime of the underlying data
buffer is managed by the ArrayBuffer even after it's returned. A
possible safe way to use this API is in conjunction with [`napi_create_reference`][],
which can be used to guarantee control over the lifetime of the
ArrayBuffer. It's also safe to use the returned data buffer within the same
callback as long as there are no calls to other APIs that might trigger a GC.

#### *napi_get_buffer_info*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_buffer_info(napi_env env,
                                 napi_value value,
                                 void** data,
                                 size_t* length)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing the `node::Buffer` being queried.
- `[out] data`: The underlying data buffer of the `node::Buffer`.
- `[out] length`: Length in bytes of the underlying data buffer.

Returns `napi_ok` if the API succeeded.

This API is used to retrieve the underlying data buffer of a `node::Buffer`
and it's length.

*Warning*: Use caution while using this API since the underlying data buffer's
lifetime is not guaranteed if it's managed by the VM.

#### *napi_get_prototype*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_prototype(napi_env env,
                               napi_value object,
                               napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] object`: `napi_value` representing JavaScript Object whose prototype
to return. This returns the equivalent of `Object.getPrototypeOf` (which is
not the same as the function's `prototype` property).
- `[out] result`: `napi_value` representing prototype of the given object.

Returns `napi_ok` if the API succeeded.

#### *napi_get_typedarray_info*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_typedarray_info(napi_env env,
                                     napi_value typedarray,
                                     napi_typedarray_type* type,
                                     size_t* length,
                                     void** data,
                                     napi_value* arraybuffer,
                                     size_t* byte_offset)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] typedarray`: `napi_value` representing the TypedArray whose
properties to query.
- `[out] type`: Scalar datatype of the elements within the TypedArray.
- `[out] length`: Number of elements in the TypedArray.
- `[out] data`: The data buffer underlying the typed array.
- `[out] byte_offset`: The byte offset within the data buffer from which
to start projecting the TypedArray.

Returns `napi_ok` if the API succeeded.

This API returns various properties of a typed array.

*Warning*: Use caution while using this API since the underlying data buffer
is managed by the VM



#### *napi_get_dataview_info*
<!-- YAML
added: v8.3.0
-->

```C
napi_status napi_get_dataview_info(napi_env env,
                                   napi_value dataview,
                                   size_t* byte_length,
                                   void** data,
                                   napi_value* arraybuffer,
                                   size_t* byte_offset)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] dataview`: `napi_value` representing the DataView whose
  properties to query.
- `[out] byte_length`: Number of bytes in the DataView.
- `[out] data`: The data buffer underlying the DataView.
- `[out] arraybuffer`: ArrayBuffer underlying the DataView.
- `[out] byte_offset`: The byte offset within the data buffer from which
  to start projecting the DataView.

Returns `napi_ok` if the API succeeded.

This API returns various properties of a DataView.


#### *napi_get_value_bool*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_bool(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript Boolean.
- `[out] result`: C boolean primitive equivalent of the given JavaScript
Boolean.

Returns `napi_ok` if the API succeeded. If a non-boolean `napi_value` is
passed in it returns `napi_boolean_expected`.

This API returns the C boolean primitive equivalent of the given JavaScript
Boolean.

#### *napi_get_value_double*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_double(napi_env env,
                                  napi_value value,
                                  double* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript Number.
- `[out] result`: C double primitive equivalent of the given JavaScript
Number.

Returns `napi_ok` if the API succeeded. If a non-number `napi_value` is passed
in it returns `napi_number_expected`.

This API returns the C double primitive equivalent of the given JavaScript
Number.


#### *napi_get_value_external*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_external(napi_env env,
                                    napi_value value,
                                    void** result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript external value.
- `[out] result`: Pointer to the data wrapped by the JavaScript external value.

Returns `napi_ok` if the API succeeded. If a non-external `napi_value` is
passed in it returns `napi_invalid_arg`.

This API retrieves the external data pointer that was previously passed to
`napi_create_external()`.

#### *napi_get_value_int32*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_int32(napi_env env,
                                 napi_value value,
                                 int32_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript Number.
- `[out] result`: C int32 primitive equivalent of the given JavaScript Number.

Returns `napi_ok` if the API succeeded. If a non-number `napi_value`
is passed in `napi_number_expected .

This API returns the C int32 primitive equivalent
of the given JavaScript Number. If the number exceeds the range of the
32 bit integer, then the result is truncated to the equivalent of the
bottom 32 bits. This can result in a large positive number becoming
a negative number if the the value is > 2^31 -1.

#### *napi_get_value_int64*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_int64(napi_env env,
                                 napi_value value,
                                 int64_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript Number.
- `[out] result`: C int64 primitive equivalent of the given JavaScript Number.

Returns `napi_ok` if the API succeeded. If a non-number `napi_value`
is passed in it returns `napi_number_expected`.

This API returns the C int64 primitive equivalent of the given
JavaScript Number

#### *napi_get_value_string_latin1*
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_get_value_string_latin1(napi_env env,
                                                     napi_value value,
                                                     char* buf,
                                                     size_t bufsize,
                                                     size_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript string.
- `[in] buf`: Buffer to write the ISO-8859-1-encoded string into. If NULL is
passed in, the length of the string (in bytes) is returned.
- `[in] bufsize`: Size of the destination buffer. When this value is
insufficient, the returned string will be truncated.
- `[out] result`: Number of bytes copied into the buffer, excluding the null
terminator.

Returns `napi_ok` if the API succeeded. If a non-String `napi_value`
is passed in it returns `napi_string_expected`.

This API returns the ISO-8859-1-encoded string corresponding the value passed
in.

#### *napi_get_value_string_utf8*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_string_utf8(napi_env env,
                                       napi_value value,
                                       char* buf,
                                       size_t bufsize,
                                       size_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript string.
- `[in] buf`: Buffer to write the UTF8-encoded string into. If NULL is passed
 in, the length of the string (in bytes) is returned.
- `[in] bufsize`: Size of the destination buffer. When this value is
insufficient, the returned string will be truncated.
- `[out] result`: Number of bytes copied into the buffer, excluding the null
terminator.

Returns `napi_ok` if the API succeeded. If a non-String `napi_value`
is passed in it returns `napi_string_expected`.

This API returns the UTF8-encoded string corresponding the value passed in.

#### *napi_get_value_string_utf16*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_string_utf16(napi_env env,
                                        napi_value value,
                                        char16_t* buf,
                                        size_t bufsize,
                                        size_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript string.
- `[in] buf`: Buffer to write the UTF16-LE-encoded string into. If NULL is
passed in, the length of the string (in 2-byte code units) is returned.
- `[in] bufsize`: Size of the destination buffer. When this value is
insufficient, the returned string will be truncated.
- `[out] result`: Number of 2-byte code units copied into the buffer, excluding the null
terminator.

Returns `napi_ok` if the API succeeded. If a non-String `napi_value`
is passed in it returns `napi_string_expected`.

This API returns the UTF16-encoded string corresponding the value passed in.

#### *napi_get_value_uint32*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_value_uint32(napi_env env,
                                  napi_value value,
                                  uint32_t* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: `napi_value` representing JavaScript Number.
- `[out] result`: C primitive equivalent of the given `napi_value` as a
`uint32_t`.

Returns `napi_ok` if the API succeeded. If a non-number `napi_value`
is passed in it returns `napi_number_expected`.

This API returns the C primitive equivalent of the given `napi_value` as a
`uint32_t`.

### Functions to get global instances
#### *napi_get_boolean*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_boolean(napi_env env, bool value, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The value of the boolean to retrieve.
- `[out] result`: `napi_value` representing JavaScript Boolean singleton to
retrieve.

Returns `napi_ok` if the API succeeded.

This API is used to return the JavaScript singleton object that is used to
represent the given boolean value

#### *napi_get_global*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_global(napi_env env, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: `napi_value` representing JavaScript Global Object.

Returns `napi_ok` if the API succeeded.

This API returns the global Object.

#### *napi_get_null*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_null(napi_env env, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: `napi_value` representing JavaScript Null Object.

Returns `napi_ok` if the API succeeded.

This API returns the null Object.

#### *napi_get_undefined*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_undefined(napi_env env, napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: `napi_value` representing JavaScript Undefined value.

Returns `napi_ok` if the API succeeded.

This API returns the Undefined object.

## Working with JavaScript Values - Abstract Operations

N-API exposes a set of APIs to perform some abstract operations on JavaScript
values. Some of these operations are documented under
[Section 7](https://tc39.github.io/ecma262/#sec-abstract-operations)
of the [ECMAScript Language Specification](https://tc39.github.io/ecma262/).

These APIs support doing one of the following:
1. Coerce JavaScript values to specific JavaScript types (such as Number or
   String)
2. Check the type of a JavaScript value
3. Check for equality between two JavaScript values

### *napi_coerce_to_bool*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_coerce_to_bool(napi_env env,
                                napi_value value,
                                napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to coerce.
- `[out] result`: `napi_value` representing the coerced JavaScript Boolean.

Returns `napi_ok` if the API succeeded.

This API implements the abstract operation ToBoolean as defined in
[Section 7.1.2](https://tc39.github.io/ecma262/#sec-toboolean)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_coerce_to_number*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_coerce_to_number(napi_env env,
                                  napi_value value,
                                  napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to coerce.
- `[out] result`: `napi_value` representing the coerced JavaScript Number.

Returns `napi_ok` if the API succeeded.

This API implements the abstract operation ToNumber as defined in
[Section 7.1.3](https://tc39.github.io/ecma262/#sec-tonumber)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_coerce_to_object*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_coerce_to_object(napi_env env,
                                  napi_value value,
                                  napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to coerce.
- `[out] result`: `napi_value` representing the coerced JavaScript Object.

Returns `napi_ok` if the API succeeded.

This API implements the abstract operation ToObject as defined in
[Section 7.1.13](https://tc39.github.io/ecma262/#sec-toobject)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_coerce_to_string*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_coerce_to_string(napi_env env,
                                  napi_value value,
                                  napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to coerce.
- `[out] result`: `napi_value` representing the coerced JavaScript String.

Returns `napi_ok` if the API succeeded.

This API implements the abstract operation ToString as defined in
[Section 7.1.13](https://tc39.github.io/ecma262/#sec-tostring)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_typeof*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value whose type to query.
- `[out] result`: The type of the JavaScript value.

Returns `napi_ok` if the API succeeded.
- `napi_invalid_arg` if the type of `value` is not a known ECMAScript type and
 `value` is not an External value.

This API represents behavior similar to invoking the `typeof` Operator on
the object as defined in [Section 12.5.5][] of the ECMAScript Language
Specification. However, it has support for detecting an External value.
If `value` has a type that is invalid, an error is returned.

### *napi_instanceof*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_instanceof(napi_env env,
                            napi_value object,
                            napi_value constructor,
                            bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] object`: The JavaScript value to check.
- `[in] constructor`: The JavaScript function object of the constructor
function to check against.
- `[out] result`: Boolean that is set to true if `object instanceof constructor`
is true.

Returns `napi_ok` if the API succeeded.

This API represents invoking the `instanceof` Operator on the object as
defined in
[Section 12.10.4](https://tc39.github.io/ecma262/#sec-instanceofoperator)
of the ECMAScript Language Specification.

### *napi_is_array*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_is_array(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to check.
- `[out] result`: Whether the given object is an array.

Returns `napi_ok` if the API succeeded.

This API represents invoking the `IsArray` operation on the object
as defined in [Section 7.2.2](https://tc39.github.io/ecma262/#sec-isarray)
of the ECMAScript Language Specification.

### *napi_is_arraybuffer*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to check.
- `[out] result`: Whether the given object is an ArrayBuffer.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passsed in is an array buffer.

### *napi_is_buffer*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_is_buffer(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to check.
- `[out] result`: Whether the given `napi_value` represents a `node::Buffer`
object.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passsed in is a buffer.

### *napi_is_error*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_is_error(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to check.
- `[out] result`: Whether the given `napi_value` represents an Error object.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passsed in is an Error.

### *napi_is_typedarray*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to check.
- `[out] result`: Whether the given `napi_value` represents a TypedArray.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passsed in is a typed array.



### *napi_is_dataview*
<!-- YAML
added: v8.3.0
-->

```C
napi_status napi_is_dataview(napi_env env, napi_value value, bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] value`: The JavaScript value to check.
- `[out] result`: Whether the given `napi_value` represents a DataView.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passed in is a DataView.

### *napi_strict_equals*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_strict_equals(napi_env env,
                               napi_value lhs,
                               napi_value rhs,
                               bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] lhs`: The JavaScript value to check.
- `[in] rhs`: The JavaScript value to check against.
- `[out] result`: Whether the two `napi_value` objects are equal.

Returns `napi_ok` if the API succeeded.

This API represents the invocation of the Strict Equality algorithm as
defined in
[Section 7.2.14](https://tc39.github.io/ecma262/#sec-strict-equality-comparison)
of the ECMAScript Language Specification.

## Working with JavaScript Properties

N-API exposes a set of APIs to get and set properties on JavaScript
objects. Some of these types are documented under
[Section 7](https://tc39.github.io/ecma262/#sec-operations-on-objects) of the
[ECMAScript Language Specification](https://tc39.github.io/ecma262/).

Properties in JavaScript are represented as a tuple of a key and a value.
Fundamentally, all property keys in N-API can be represented in one of the
following forms:
- Named: a simple UTF8-encoded string
- Integer-Indexed: an index value represented by `uint32_t`
- JavaScript value: these are represented in N-API by `napi_value`. This can
be a `napi_value` representing a String, Number or Symbol.

N-API values are represented by the type `napi_value`.
Any N-API call that requires a JavaScript value takes in a `napi_value`.
However, it's the caller's responsibility to make sure that the
`napi_value` in question is of the JavaScript type expected by the API.

The APIs documented in this section provide a simple interface to
get and set properties on arbitrary JavaScript objects represented by
`napi_value`.

For instance, consider the following JavaScript code snippet:
```js
const obj = {};
obj.myProp = 123;
```
The equivalent can be done using N-API values with the following snippet:
```C
napi_status status = napi_generic_failure;

// const obj = {}
napi_value obj, value;
status = napi_create_object(env, &obj);
if (status != napi_ok) return status;

// Create a napi_value for 123
status = napi_create_int32(env, 123, &value);
if (status != napi_ok) return status;

// obj.myProp = 123
status = napi_set_named_property(env, obj, "myProp", value);
if (status != napi_ok) return status;
```

Indexed properties can be set in a similar manner. Consider the following
JavaScript snippet:
```js
const arr = [];
arr[123] = 'hello';
```
The equivalent can be done using N-API values with the following snippet:
```C
napi_status status = napi_generic_failure;

// const arr = [];
napi_value arr, value;
status = napi_create_array(env, &arr);
if (status != napi_ok) return status;

// Create a napi_value for 'hello'
status = napi_create_string_utf8(env, "hello", -1, &value);
if (status != napi_ok) return status;

// arr[123] = 'hello';
status = napi_set_element(env, arr, 123, value);
if (status != napi_ok) return status;
```

Properties can be retrieved using the APIs described in this section.
Consider the following JavaScript snippet:
```js
const arr = [];
const value = arr[123];
```

The following is the approximate equivalent of the N-API counterpart:
```C
napi_status status = napi_generic_failure;

// const arr = []
napi_value arr, value;
status = napi_create_array(env, &arr);
if (status != napi_ok) return status;

// const value = arr[123]
status = napi_get_element(env, arr, 123, &value);
if (status != napi_ok) return status;
```

Finally, multiple properties can also be defined on an object for performance
reasons. Consider the following JavaScript:
```js
const obj = {};
Object.defineProperties(obj, {
  'foo': { value: 123, writable: true, configurable: true, enumerable: true },
  'bar': { value: 456, writable: true, configurable: true, enumerable: true }
});
```

The following is the approximate equivalent of the N-API counterpart:
```C
napi_status status = napi_status_generic_failure;

// const obj = {};
napi_value obj;
status = napi_create_obj(env, &obj);
if (status != napi_ok) return status;

// Create napi_values for 123 and 456
napi_value fooValue, barValue;
status = napi_create_int32(env, 123, &fooValue);
if (status != napi_ok) return status;
status = napi_create_int32(env, 456, &barValue);
if (status != napi_ok) return status;

// Set the properties
napi_property_descriptors descriptors[] = {
  { "foo", fooValue, 0, 0, 0, napi_default, 0 },
  { "bar", barValue, 0, 0, 0, napi_default, 0 }
}
status = napi_define_properties(env,
                                obj,
                                sizeof(descriptors) / sizeof(descriptors[0]),
                                descriptors);
if (status != napi_ok) return status;
```

### Structures
#### *napi_property_attributes*
```C
typedef enum {
  napi_default = 0,
  napi_writable = 1 << 0,
  napi_enumerable = 1 << 1,
  napi_configurable = 1 << 2,

  // Used with napi_define_class to distinguish static properties
  // from instance properties. Ignored by napi_define_properties.
  napi_static = 1 << 10,
} napi_property_attributes;
```

`napi_property_attributes` are flags used to control the behavior of properties
set on a JavaScript object. Other than `napi_static` they correspond to the
attributes listed in [Section 6.1.7.1](https://tc39.github.io/ecma262/#table-2)
of the [ECMAScript Language Specification](https://tc39.github.io/ecma262/).
They can be one or more of the following bitflags:

- `napi_default` - Used to indicate that no explicit attributes are set on the
given property. By default, a property is read only, not enumerable and not
configurable.
- `napi_writable`  - Used to indicate that a given property is writable.
- `napi_enumerable` - Used to indicate that a given property is enumerable.
- `napi_configurable` - Used to indicate that a given property is
configurable, as defined in
[Section 6.1.7.1](https://tc39.github.io/ecma262/#table-2) of the
[ECMAScript Language Specification](https://tc39.github.io/ecma262/).
- `napi_static` - Used to indicate that the property will be defined as
a static property on a class as opposed to an instance property, which is the
default. This is used only by [`napi_define_class`][]. It is ignored by
`napi_define_properties`.

#### *napi_property_descriptor*
```C
typedef struct {
  // One of utf8name or name should be NULL.
  const char* utf8name;
  napi_value name;

  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;

  napi_property_attributes attributes;
  void* data;
} napi_property_descriptor;
```

- `utf8name`: Optional String describing the key for the property,
encoded as UTF8. One of `utf8name` or `name` must be provided for the
property.
- `name`: Optional napi_value that points to a JavaScript string or symbol
to be used as the key for the property.  One of `utf8name` or `name` must
be provided for the property.
- `value`: The value that's retrieved by a get access of the property if the
 property is a data property. If this is passed in, set `getter`, `setter`,
 `method` and `data` to `NULL` (since these members won't be used).
- `getter`: A function to call when a get access of the property is performed.
If this is passed in, set `value` and `method` to `NULL` (since these members
won't be used). The given function is called implicitly by the runtime when the
property is accessed from JavaScript code (or if a get on the property is
performed using a N-API call).
- `setter`: A function to call when a set access of the property is performed.
If this is passed in, set `value` and `method` to `NULL` (since these members
won't be used). The given function is called implicitly by the runtime when the
property is set from JavaScript code (or if a set on the property is
performed using a N-API call).
- `method`: Set this to make the property descriptor object's `value`
property to be a JavaScript function represented by `method`. If this is
passed in, set `value`, `getter` and `setter` to `NULL` (since these members
won't be used).
- `data`: The callback data passed into `method`, `getter` and `setter` if
this function is invoked.
- `attributes`: The attributes associated with the particular property.
See [`napi_property_attributes`](#n_api_napi_property_attributes).

### Functions
#### *napi_get_property_names*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_property_names(napi_env env,
                                    napi_value object,
                                    napi_value* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object from which to retrieve the properties.
- `[out] result`: A `napi_value` representing an array of JavaScript values
that represent the property names of the object. The API can be used to
iterate over `result` using [`napi_get_array_length`][]
and [`napi_get_element`][].

Returns `napi_ok` if the API succeeded.

This API returns the array of propertys for the Object passed in

#### *napi_set_property*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_set_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value value);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object on which to set the property.
- `[in] key`: The name of the property to set.
- `[in] value`: The property value.

Returns `napi_ok` if the API succeeded.

This API set a property on the Object passed in.

#### *napi_get_property*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object from which to retrieve the property.
- `[in] key`: The name of the property to retrieve.
- `[out] result`: The value of the property.

Returns `napi_ok` if the API succeeded.

This API gets the requested property from the Object passed in.


#### *napi_has_property*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_has_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              bool* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object to query.
- `[in] key`: The name of the property whose existence to check.
- `[out] result`: Whether the property exists on the object or not.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passed in has the named property.


#### *napi_delete_property*
<!-- YAML
added: v8.2.0
-->
```C
napi_status napi_delete_property(napi_env env,
                                 napi_value object,
                                 napi_value key,
                                 bool* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object to query.
- `[in] key`: The name of the property to delete.
- `[out] result`: Whether the property deletion succeeded or not. `result` can
optionally be ignored by passing `NULL`.

Returns `napi_ok` if the API succeeded.

This API attempts to delete the `key` own property from `object`.


#### *napi_has_own_property*
<!-- YAML
added: v8.2.0
-->
```C
napi_status napi_has_own_property(napi_env env,
                                  napi_value object,
                                  napi_value key,
                                  bool* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object to query.
- `[in] key`: The name of the own property whose existence to check.
- `[out] result`: Whether the own property exists on the object or not.

Returns `napi_ok` if the API succeeded.

This API checks if the Object passed in has the named own property. `key` must
be a string or a Symbol, or an error will be thrown. N-API will not perform any
conversion between data types.


#### *napi_set_named_property*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_set_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    napi_value value);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object on which to set the property.
- `[in] utf8Name`: The name of the property to set.
- `[in] value`: The property value.

Returns `napi_ok` if the API succeeded.

This method is equivalent to calling [`napi_set_property`][] with a `napi_value`
created from the string passed in as `utf8Name`

#### *napi_get_named_property*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    napi_value* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object from which to retrieve the property.
- `[in] utf8Name`: The name of the property to get.
- `[out] result`: The value of the property.

Returns `napi_ok` if the API succeeded.

This method is equivalent to calling [`napi_get_property`][] with a `napi_value`
created from the string passed in as `utf8Name`

#### *napi_has_named_property*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_has_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    bool* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object to query.
- `[in] utf8Name`: The name of the property whose existence to check.
- `[out] result`: Whether the property exists on the object or not.

Returns `napi_ok` if the API succeeded.

This method is equivalent to calling [`napi_has_property`][] with a `napi_value`
created from the string passed in as `utf8Name`

#### *napi_set_element*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_set_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             napi_value value);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object from which to set the properties.
- `[in] index`: The index of the property to set.
- `[in] value`: The property value.

Returns `napi_ok` if the API succeeded.

This API sets and element on the Object passed in.

#### *napi_get_element*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             napi_value* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object from which to retrieve the property.
- `[in] index`: The index of the property to get.
- `[out] result`: The value of the property.

Returns `napi_ok` if the API succeeded.

This API gets the element at the requested index.

#### *napi_has_element*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_has_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             bool* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object to query.
- `[in] index`: The index of the property whose existence to check.
- `[out] result`: Whether the property exists on the object or not.

Returns `napi_ok` if the API succeeded.

This API returns if the Object passed in has an element at the
requested index.

#### *napi_delete_element*
<!-- YAML
added: v8.2.0
-->
```C
napi_status napi_delete_element(napi_env env,
                                napi_value object,
                                uint32_t index,
                                bool* result);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object to query.
- `[in] index`: The index of the property to delete.
- `[out] result`: Whether the element deletion succeeded or not. `result` can
optionally be ignored by passing `NULL`.

Returns `napi_ok` if the API succeeded.

This API attempts to delete the specified `index` from `object`.

#### *napi_define_properties*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_define_properties(napi_env env,
                                   napi_value object,
                                   size_t property_count,
                                   const napi_property_descriptor* properties);
```

- `[in] env`: The environment that the N-API call is invoked under.
- `[in] object`: The object from which to retrieve the properties.
- `[in] property_count`: The number of elements in the `properties` array.
- `[in] properties`: The array of property descriptors.

Returns `napi_ok` if the API succeeded.

This method allows the efficient definition of multiple properties on a given
object. The properties are defined using property descriptors (See
[`napi_property_descriptor`][]). Given an array of such property descriptors, this
API will set the properties on the object one at a time, as defined by
DefineOwnProperty (described in [Section 9.1.6][] of the ECMA262 specification).

## Working with JavaScript Functions

N-API provides a set of APIs that allow JavaScript code to
call back into native code. N-API APIs that support calling back
into native code take in a callback functions represented by
the `napi_callback` type. When the JavaScript VM calls back to
native code, the `napi_callback` function provided is invoked. The APIs
documented in this section allow the callback function to do the
following:
- Get information about the context in which the callback was invoked.
- Get the arguments passed into the callback.
- Return a `napi_value` back from the callback.

Additionally, N-API provides a set of functions which allow calling
JavaScript functions from native code. One can either call a function
like a regular JavaScript function call, or as a constructor
function.


### *napi_call_function*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_call_function(napi_env env,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               const napi_value* argv,
                               napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] recv`: The `this` object passed to the called function.
- `[in] func`: `napi_value` representing the JavaScript function
to be invoked.
- `[in] argc`: The count of elements in the `argv` array.
- `[in] argv`: Array of `napi_values` representing JavaScript values passed
in as arguments to the function.
- `[out] result`: `napi_value` representing the JavaScript object returned.

Returns `napi_ok` if the API succeeded.

This method allows a JavaScript function object to be called from a native
add-on. This is an primary mechanism of calling back *from* the add-on's
native code *into* JavaScript. For special cases like calling into JavaScript
after an async operation, see [`napi_make_callback`][].

A sample use case might look as follows. Consider the following JavaScript
snippet:
```js
function AddTwo(num) {
  return num + 2;
}
```

Then, the above function can be invoked from a native add-on using the
following code:
```C
// Get the function named "AddTwo" on the global object
napi_value global, add_two, arg;
napi_status status = napi_get_global(env, &global);
if (status != napi_ok) return;

status = napi_get_named_property(env, global, "AddTwo", &add_two);
if (status != napi_ok) return;

// const arg = 1337
status = napi_create_int32(env, 1337, &arg);
if (status != napi_ok) return;

napi_value* argv = &arg;
size_t argc = 1;

// AddTwo(arg);
napi_value return_val;
status = napi_call_function(env, global, add_two, argc, argv, &return_val);
if (status != napi_ok) return;

// Convert the result back to a native type
int32_t result;
status = napi_get_value_int32(env, return_val, &result);
if (status != napi_ok) return;
```

### *napi_create_function*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_create_function(napi_env env,
                                 const char* utf8name,
                                 napi_callback cb,
                                 void* data,
                                 napi_value* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] utf8Name`: The name of the function encoded as UTF8. This is visible
within JavaScript as the new function object's `name` property.
- `[in] cb`: The native function which should be called when this function
object is invoked.
- `[in] data`: User-provided data context. This will be passed back into the
function when invoked later.
- `[out] result`: `napi_value` representing the JavaScript function object for
the newly created function.

Returns `napi_ok` if the API succeeded.

This API allows an add-on author to create a function object in native code.
This is the primary mechanism to allow calling *into* the add-on's native code
*from* Javascript.

*Note*: The newly created function is not automatically visible from
script after this call. Instead, a property must be explicitly set on any
object that is visible to JavaScript, in order for the function to be accessible
from script.

In order to expose a function as part of the
add-on's module exports, set the newly created function on the exports
object. A sample module might look as follows:
```C
napi_value SayHello(napi_env env, napi_callback_info info) {
  printf("Hello\n");
  return nullptr;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value fn;
  status =  napi_create_function(env, NULL, SayHello, NULL, &fn);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, exports, "sayHello", fn);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
```

Given the above code, the add-on can be used from JavaScript as follows:
```js
const myaddon = require('./addon');
myaddon.sayHello();
```

*Note*: The string passed to require is not necessarily the name passed into
`NAPI_MODULE` in the earlier snippet but the name of the target in `binding.gyp`
responsible for creating the `.node` file.

### *napi_get_cb_info*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_get_cb_info(napi_env env,
                             napi_callback_info cbinfo,
                             size_t* argc,
                             napi_value* argv,
                             napi_value* thisArg,
                             void** data)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] cbinfo`: The callback info passed into the callback function.
- `[in-out] argc`: Specifies the size of the provided `argv` array
and receives the actual count of arguments.
- `[out] argv`: Buffer to which the `napi_value` representing the
arguments are copied. If there are more arguments than the provided
count, only the requested number of arguments are copied. If there are fewer
arguments provided than claimed, the rest of `argv` is filled with `napi_value`
values that represent `undefined`.
- `[out] this`: Receives the JavaScript `this` argument for the call.
- `[out] data`: Receives the data pointer for the callback.

Returns `napi_ok` if the API succeeded.

This method is used within a callback function to retrieve details about the
call like the arguments and the `this` pointer from a given callback info.

### *napi_is_construct_call*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_is_construct_call(napi_env env,
                                   napi_callback_info cbinfo,
                                   bool* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] cbinfo`: The callback info passed into the callback function.
- `[out] result`: Whether the native function is being invoked as
a constructor call.

Returns `napi_ok` if the API succeeded.

This API checks if the the current callback was due to a
consructor call.

### *napi_new_instance*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_new_instance(napi_env env,
                              napi_value cons,
                              size_t argc,
                              napi_value* argv,
                              napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] cons`: `napi_value` representing the JavaScript function
to be invoked as a constructor.
- `[in] argc`: The count of elements in the `argv` array.
- `[in] argv`: Array of JavaScript values as `napi_value`
representing the arguments to the constructor.
- `[out] result`: `napi_value` representing the JavaScript object returned,
which in this case is the constructed object.

This method is used to instantiate a new JavaScript value using a given
`napi_value` that represents the constructor for the object. For example,
consider the following snippet:
```js
function MyObject(param) {
  this.param = param;
}

const arg = 'hello';
const value = new MyObject(arg);
```

The following can be approximated in N-API using the following snippet:
```C
// Get the constructor function MyObject
napi_value global, constructor, arg, value;
napi_status status = napi_get_global(env, &global);
if (status != napi_ok) return;

status = napi_get_named_property(env, global, "MyObject", &constructor);
if (status != napi_ok) return;

// const arg = "hello"
status = napi_create_string_utf8(env, "hello", -1, &arg);
if (status != napi_ok) return;

napi_value* argv = &arg;
size_t argc = 1;

// const value = new MyObject(arg)
status = napi_new_instance(env, constructor, argc, argv, &value);
```

Returns `napi_ok` if the API succeeded.

### *napi_make_callback*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_make_callback(napi_env env,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               const napi_value* argv,
                               napi_value* result)
```

- `[in] env`: The environment that the API is invoked under.
- `[in] recv`: The `this` object passed to the called function.
- `[in] func`: `napi_value` representing the JavaScript function
to be invoked.
- `[in] argc`: The count of elements in the `argv` array.
- `[in] argv`: Array of JavaScript values as `napi_value`
representing the arguments to the function.
- `[out] result`: `napi_value` representing the JavaScript object returned.

Returns `napi_ok` if the API succeeded.

This method allows a JavaScript function object to be called from a native
add-on. This API is similar to `napi_call_function`. However, it is used to call
*from* native code back *into* JavaScript *after* returning from an async
operation (when there is no other script on the stack). It is a fairly simple
wrapper around `node::MakeCallback`.

For an example on how to use `napi_make_callback`, see the section on
[Asynchronous Operations][].

## Object Wrap

N-API offers a way to "wrap" C++ classes and instances so that the class
constructor and methods can be called from JavaScript.

 1. The [`napi_define_class`][] API defines a JavaScript class with constructor,
    static properties and methods, and instance properties and methods that
    correspond to the The C++ class.
 2. When JavaScript code invokes the constructor, the constructor callback
    uses [`napi_wrap`][] to wrap a new C++ instance in a JavaScript object,
    then returns the wrapper object.
 3. When JavaScript code invokes a method or property accessor on the class,
    the corresponding `napi_callback` C++ function is invoked. For an instance
    callback, [`napi_unwrap`][] obtains the C++ instance that is the target of
    the call.

### *napi_define_class*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_define_class(napi_env env,
                              const char* utf8name,
                              napi_callback constructor,
                              void* data,
                              size_t property_count,
                              const napi_property_descriptor* properties,
                              napi_value* result);
```

 - `[in] env`: The environment that the API is invoked under.
 - `[in] utf8name`: Name of the JavaScript constructor function; this is
   not required to be the same as the C++ class name, though it is recommended
   for clarity.
 - `[in] constructor`: Callback function that handles constructing instances
   of the class. (This should be a static method on the class, not an actual
   C++ constructor function.)
 - `[in] data`: Optional data to be passed to the constructor callback as
   the `data` property of the callback info.
 - `[in] property_count`: Number of items in the `properties` array argument.
 - `[in] properties`: Array of property descriptors describing static and
   instance data properties, accessors, and methods on the class
   See `napi_property_descriptor`.
 - `[out] result`: A `napi_value` representing the constructor function for
   the class.

Returns `napi_ok` if the API succeeded.

Defines a JavaScript class that corresponds to a C++ class, including:
 - A JavaScript constructor function that has the class name and invokes the
   provided C++ constructor callback.
 - Properties on the constructor function corresponding to _static_ data
   properties, accessors, and methods of the C++ class (defined by
   property descriptors with the `napi_static` attribute).
 - Properties on the constructor function's `prototype` object corresponding to
   _non-static_ data properties, accessors, and methods of the C++ class
   (defined by property descriptors without the `napi_static` attribute).

The C++ constructor callback should be a static method on the class that calls
the actual class constructor, then wraps the new C++ instance in a JavaScript
object, and returns the wrapper object. See `napi_wrap()` for details.

The JavaScript constructor function returned from [`napi_define_class`][] is
often saved and used later, to construct new instances of the class from native
code, and/or check whether provided values are instances of the class. In that
case, to prevent the function value from being garbage-collected, create a
persistent reference to it using [`napi_create_reference`][] and ensure the
reference count is kept >= 1.

### *napi_wrap*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_wrap(napi_env env,
                      napi_value js_object,
                      void* native_object,
                      napi_finalize finalize_cb,
                      void* finalize_hint,
                      napi_ref* result);
```

 - `[in] env`: The environment that the API is invoked under.
 - `[in] js_object`: The JavaScript object that will be the wrapper for the
   native object. This object _must_ have been created from the `prototype` of
   a constructor that was created using `napi_define_class()`.
 - `[in] native_object`: The native instance that will be wrapped in the
   JavaScript object.
 - `[in] finalize_cb`: Optional native callback that can be used to free the
   native instance when the JavaScript object is ready for garbage-collection.
 - `[in] finalize_hint`: Optional contextual hint that is passed to the
   finalize callback.
 - `[out] result`: Optional reference to the wrapped object.

Returns `napi_ok` if the API succeeded.

Wraps a native instance in a JavaScript object. The native instance can be
retrieved later using `napi_unwrap()`.

When JavaScript code invokes a constructor for a class that was defined using
`napi_define_class()`, the `napi_callback` for the constructor is invoked.
After constructing an instance of the native class, the callback must then call
`napi_wrap()` to wrap the newly constructed instance in the already-created
JavaScript object that is the `this` argument to the constructor callback.
(That `this` object was created from the constructor function's `prototype`,
so it already has definitions of all the instance properties and methods.)

Typically when wrapping a class instance, a finalize callback should be
provided that simply deletes the native instance that is received as the `data`
argument to the finalize callback.

The optional returned reference is initially a weak reference, meaning it
has a reference count of 0. Typically this reference count would be incremented
temporarily during async operations that require the instance to remain valid.

*Caution*: The optional returned reference (if obtained) should be deleted via
[`napi_delete_reference`][] ONLY in response to the finalize callback
invocation. (If it is deleted before then, then the finalize callback may never
be invoked.) Therefore, when obtaining a reference a finalize callback is also
required in order to enable correct proper of the reference.

*Note*: This API may modify the prototype chain of the wrapper object.
Afterward, additional manipulation of the wrapper's prototype chain may cause
`napi_unwrap()` to fail.

*Note*: Calling `napi_wrap()` a second time on an object that already has a
native instance associated with it by virtue of a previous call to
`napi_wrap()` will cause an error to be returned. If you wish to associate
another native instance with the given object, call `napi_remove_wrap()` on it
first.

### *napi_unwrap*
<!-- YAML
added: v8.0.0
-->
```C
napi_status napi_unwrap(napi_env env,
                        napi_value js_object,
                        void** result);
```

 - `[in] env`: The environment that the API is invoked under.
 - `[in] js_object`: The object associated with the native instance.
 - `[out] result`: Pointer to the wrapped native instance.

Returns `napi_ok` if the API succeeded.

Retrieves a native instance that was previously wrapped in a JavaScript
object using `napi_wrap()`.

When JavaScript code invokes a method or property accessor on the class, the
corresponding `napi_callback` is invoked. If the callback is for an instance
method or accessor, then the `this` argument to the callback is the wrapper
object; the wrapped C++ instance that is the target of the call can be obtained
then by calling `napi_unwrap()` on the wrapper object.

### *napi_remove_wrap*
<!-- YAML
added: REPLACEME
-->
```C
napi_status napi_remove_wrap(napi_env env,
                             napi_value js_object,
                             void** result);
```

 - `[in] env`: The environment that the API is invoked under.
 - `[in] js_object`: The object associated with the native instance.
 - `[out] result`: Pointer to the wrapped native instance.

Returns `napi_ok` if the API succeeded.

Retrieves a native instance that was previously wrapped in the JavaScript
object `js_object` using `napi_wrap()` and removes the wrapping, thereby
restoring the JavaScript object's prototype chain. If a finalize callback was
associated with the wrapping, it will no longer be called when the JavaScript
object becomes garbage-collected.

## Asynchronous Operations

Addon modules often need to leverage async helpers from libuv as part of their
implementation. This allows them to schedule work to be executed asynchronously
so that their methods can return in advance of the work being completed. This
is important in order to allow them to avoid blocking overall execution
of the Node.js application.

N-API provides an ABI-stable interface for these
supporting functions which covers the most common asynchronous use cases.

N-API defines the `napi_work` structure which is used to manage
asynchronous workers. Instances are created/deleted with
[`napi_create_async_work`][] and [`napi_delete_async_work`][].

The `execute` and `complete` callbacks are functions that will be
invoked when the executor is ready to execute and when it completes its
task respectively. These functions implement the following interfaces:

```C
typedef void (*napi_async_execute_callback)(napi_env env,
                                            void* data);
typedef void (*napi_async_complete_callback)(napi_env env,
                                             napi_status status,
                                             void* data);
```


When these methods are invoked, the `data` parameter passed will be the
addon-provided void* data that was passed into the
`napi_create_async_work` call.

Once created the async worker can be queued
for execution using the [`napi_queue_async_work`][] function:

```C
NAPI_EXTERN napi_status napi_queue_async_work(napi_env env,
                                              napi_async_work work);
```

[`napi_cancel_async_work`][] can be used if  the work needs
to be cancelled before the work has started execution.

After calling [`napi_cancel_async_work`][], the `complete` callback
will be invoked with a status value of `napi_cancelled`.
The work should not be deleted before the `complete`
callback invocation, even when it was cancelled.

### napi_create_async_work
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN
napi_status napi_create_async_work(napi_env env,
                                   napi_async_execute_callback execute,
                                   napi_async_complete_callback complete,
                                   void* data,
                                   napi_async_work* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] execute`: The native function which should be called to excute
the logic asynchronously.
- `[in] complete`: The native function which will be called when the
asynchronous logic is comple or is cancelled.
- `[in] data`: User-provided data context. This will be passed back into the
execute and complete functions.
- `[out] result`: `napi_async_work*` which is the handle to the newly created
async work.

Returns `napi_ok` if the API succeeded.

This API allocates a work object that is used to execute logic asynchronously.
It should be freed using [`napi_delete_async_work`][] once the work is no longer
required.

### napi_delete_async_work
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_delete_async_work(napi_env env,
                                               napi_async_work work);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] work`: The handle returned by the call to `napi_create_async_work`.

Returns `napi_ok` if the API succeeded.

This API frees a previously allocated work object.

### napi_queue_async_work
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_queue_async_work(napi_env env,
                                              napi_async_work work);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] work`: The handle returned by the call to `napi_create_async_work`.

Returns `napi_ok` if the API succeeded.

This API requests that the previously allocated work be scheduled
for execution.

### napi_cancel_async_work
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_cancel_async_work(napi_env env,
                                               napi_async_work work);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] work`: The handle returned by the call to `napi_create_async_work`.

Returns `napi_ok` if the API succeeded.

This API cancels queued work if it has not yet
been started.  If it has already started executing, it cannot be
cancelled and `napi_generic_failure` will be returned. If successful,
the `complete` callback will be invoked with a status value of
`napi_cancelled`. The work should not be deleted before the `complete`
callback invocation, even if it has been successfully cancelled.

## Version Management

### napi_get_node_version
<!-- YAML
added: v8.4.0
-->

```C
typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
} napi_node_version;

NAPI_EXTERN
napi_status napi_get_node_version(napi_env env,
                                  const napi_node_version** version);
```

- `[in] env`: The environment that the API is invoked under.
- `[out] version`: A pointer to version information for Node itself.

Returns `napi_ok` if the API succeeded.

This function fills the `version` struct with the major, minor and patch version
of Node that is currently running, and the `release` field with the
value of [`process.release.name`][`process.release`].

The returned buffer is statically allocated and does not need to be freed.

### napi_get_version
<!-- YAML
added: v8.0.0
-->
```C
NAPI_EXTERN napi_status napi_get_version(napi_env env,
                                         uint32_t* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[out] result`: The highest version of N-API supported.

Returns `napi_ok` if the API succeeded.

This API returns the highest N-API version supported by the
Node.js runtime.  N-API is planned to be additive such that
newer releases of Node.js may support additional API functions.
In order to allow an addon to use a newer function when running with
versions of Node.js that support it, while providing
fallback behavior when running with Node.js versions that don't
support it:

* Call `napi_get_version()` to determine if the API is available.
* If available, dynamically load a pointer to the function using `uv_dlsym()`.
* Use the dynamically loaded pointer to invoke the function.
* If the function is not available, provide an alternate implementation
  that does not use the function.

## Memory Management

### napi_adjust_external_memory
<!-- YAML
added: REPLACEME
-->
```C
NAPI_EXTERN napi_status napi_adjust_external_memory(napi_env env,
                                                    int64_t change_in_bytes,
                                                    int64_t* result);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] change_in_bytes`: The change in externally allocated memory that is
kept alive by JavaScript objects.
- `[out] result`: The adjusted value

Returns `napi_ok` if the API succeeded.

This function gives V8 an indication of the amount of externally allocated
memory that is kept alive by JavaScript objects (i.e. a JavaScript object
that points to its own memory allocated by a native module). Registering
externally allocated memory will trigger global garbage collections more
often than it would otherwise.

<!-- it's very convenient to have all the anchors indexed -->
<!--lint disable no-unused-definitions remark-lint-->
## Promises

N-API provides facilities for creating `Promise` objects as described in
[Section 25.4][] of the ECMA specification. It implements promises as a pair of
objects. When a promise is created by `napi_create_promise()`, a "deferred"
object is created and returned alongside the `Promise`. The deferred object is
bound to the created `Promise` and is the only means to resolve or reject the
`Promise` using `napi_resolve_deferred()` or `napi_reject_deferred()`. The
deferred object that is created by `napi_create_promise()` is freed by
`napi_resolve_deferred()` or `napi_reject_deferred()`. The `Promise` object may
be returned to JavaScript where it can be used in the usual fashion.

For example, to create a promise and pass it to an asynchronous worker:
```c
napi_deferred deferred;
napi_value promise;
napi_status status;

// Create the promise.
status = napi_create_promise(env, &deferred, &promise);
if (status != napi_ok) return NULL;

// Pass the deferred to a function that performs an asynchronous action.
do_something_asynchronous(deferred);

// Return the promise to JS
return promise;
```

The above function `do_something_asynchronous()` would perform its asynchronous
action and then it would resolve or reject the deferred, thereby concluding the
promise and freeing the deferred:
```c
napi_deferred deferred;
napi_value undefined;
napi_status status;

// Create a value with which to conclude the deferred.
status = napi_get_undefined(env, &undefined);
if (status != napi_ok) return NULL;

// Resolve or reject the promise associated with the deferred depending on
// whether the asynchronous action succeeded.
if (asynchronous_action_succeeded) {
  status = napi_resolve_deferred(env, deferred, undefined);
} else {
  status = napi_reject_deferred(env, deferred, undefined);
}
if (status != napi_ok) return NULL;

// At this point the deferred has been freed, so we should assign NULL to it.
deferred = NULL;
```

### napi_create_promise
<!-- YAML
added: REPLACEME
-->
```C
NAPI_EXTERN napi_status napi_create_promise(napi_env env,
                                            napi_deferred* deferred,
                                            napi_value* promise);
```

- `[in] env`: The environment that the API is invoked under.
- `[out] deferred`: A newly created deferred object which can later be passed to
`napi_resolve_deferred()` or `napi_reject_deferred()` to resolve resp. reject
the associated promise.
- `[out] promise`: The JavaScript promise associated with the deferred object.

Returns `napi_ok` if the API succeeded.

This API creates a deferred object and a JavaScript promise.

### napi_resolve_deferred
<!-- YAML
added: REPLACEME
-->
```C
NAPI_EXTERN napi_status napi_resolve_deferred(napi_env env,
                                              napi_deferred deferred,
                                              napi_value resolution);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] deferred`: The deferred object whose associated promise to resolve.
- `[in] resolution`: The value with which to resolve the promise.

This API resolves a JavaScript promise by way of the deferred object
with which it is associated. Thus, it can only be used to resolve JavaScript
promises for which the corresponding deferred object is available. This
effectively means that the promise must have been created using
`napi_create_promise()` and the deferred object returned from that call must
have been retained in order to be passed to this API.

The deferred object is freed upon successful completion.

### napi_reject_deferred
<!-- YAML
added: REPLACEME
-->
```C
NAPI_EXTERN napi_status napi_reject_deferred(napi_env env,
                                             napi_deferred deferred,
                                             napi_value rejection);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] deferred`: The deferred object whose associated promise to resolve.
- `[in] rejection`: The value with which to reject the promise.

This API rejects a JavaScript promise by way of the deferred object
with which it is associated. Thus, it can only be used to reject JavaScript
promises for which the corresponding deferred object is available. This
effectively means that the promise must have been created using
`napi_create_promise()` and the deferred object returned from that call must
have been retained in order to be passed to this API.

The deferred object is freed upon successful completion.

### napi_is_promise
<!-- YAML
added: REPLACEME
-->
```C
NAPI_EXTERN napi_status napi_is_promise(napi_env env,
                                        napi_value promise,
                                        bool* is_promise);
```

- `[in] env`: The environment that the API is invoked under.
- `[in] promise`: The promise to examine
- `[out] is_promise`: Flag indicating whether `promise` is a native promise
object - that is, a promise object created by the underlying engine.

[Promises]: #n_api_promises
[Asynchronous Operations]: #n_api_asynchronous_operations
[Basic N-API Data Types]: #n_api_basic_n_api_data_types
[ECMAScript Language Specification]: https://tc39.github.io/ecma262/
[Error Handling]: #n_api_error_handling
[Module Registration]: #n_api_module_registration
[Native Abstractions for Node.js]: https://github.com/nodejs/nan
[Object Lifetime Management]: #n_api_object_lifetime_management
[Object Wrap]: #n_api_object_wrap
[Section 9.1.6]: https://tc39.github.io/ecma262/#sec-ordinary-object-internal-methods-and-internal-slots-defineownproperty-p-desc
[Section 12.5.5]: https://tc39.github.io/ecma262/#sec-typeof-operator
[Section 24.3]: https://tc39.github.io/ecma262/#sec-dataview-objects
[Section 25.4]: https://tc39.github.io/ecma262/#sec-promise-objects
[Working with JavaScript Functions]: #n_api_working_with_javascript_functions
[Working with JavaScript Properties]: #n_api_working_with_javascript_properties
[Working with JavaScript Values]: #n_api_working_with_javascript_values
[Working with JavaScript Values - Abstract Operations]: #n_api_working_with_javascript_values_abstract_operations

[`napi_cancel_async_work`]: #n_api_napi_cancel_async_work
[`napi_close_escapable_handle_scope`]: #n_api_napi_close_escapable_handle_scope
[`napi_close_handle_scope`]: #n_api_napi_close_handle_scope
[`napi_create_async_work`]: #n_api_napi_create_async_work
[`napi_create_error`]: #n_api_napi_create_error
[`napi_create_external_arraybuffer`]: #n_api_napi_create_external_arraybuffer
[`napi_create_range_error`]: #n_api_napi_create_range_error
[`napi_create_reference`]: #n_api_napi_create_reference
[`napi_create_type_error`]: #n_api_napi_create_type_error
[`napi_delete_async_work`]: #n_api_napi_delete_async_work
[`napi_define_class`]: #n_api_napi_define_class
[`napi_delete_element`]: #n_api_napi_delete_element
[`napi_delete_property`]: #n_api_napi_delete_property
[`napi_delete_reference`]: #n_api_napi_delete_reference
[`napi_escape_handle`]: #n_api_napi_escape_handle
[`napi_get_array_length`]: #n_api_napi_get_array_length
[`napi_get_element`]: #n_api_napi_get_element
[`napi_get_property`]: #n_api_napi_get_property
[`napi_has_property`]: #n_api_napi_has_property
[`napi_has_own_property`]: #n_api_napi_has_own_property
[`napi_set_property`]: #n_api_napi_set_property
[`napi_get_reference_value`]: #n_api_napi_get_reference_value
[`napi_is_error`]: #n_api_napi_is_error
[`napi_is_exception_pending`]: #n_api_napi_is_exception_pending
[`napi_get_last_error_info`]: #n_api_napi_get_last_error_info
[`napi_get_and_clear_last_exception`]: #n_api_napi_get_and_clear_last_exception
[`napi_make_callback`]: #n_api_napi_make_callback
[`napi_open_escapable_handle_scope`]: #n_api_napi_open_escapable_handle_scope
[`napi_open_handle_scope`]: #n_api_napi_open_handle_scope
[`napi_property_descriptor`]: #n_api_napi_property_descriptor
[`napi_queue_async_work`]: #n_api_napi_queue_async_work
[`napi_reference_ref`]: #n_api_napi_reference_ref
[`napi_reference_unref`]: #n_api_napi_reference_unref
[`napi_throw`]: #n_api_napi_throw
[`napi_throw_error`]: #n_api_napi_throw_error
[`napi_throw_range_error`]: #n_api_napi_throw_range_error
[`napi_throw_type_error`]: #n_api_napi_throw_type_error
[`napi_unwrap`]: #n_api_napi_unwrap
[`napi_wrap`]: #n_api_napi_wrap

[`process.release`]: process.html#process_process_release
