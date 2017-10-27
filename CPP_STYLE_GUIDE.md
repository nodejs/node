# C++ Style Guide

Unfortunately, the C++ linter (based on
[Google’s `cpplint`](https://github.com/google/styleguide)), which can be run
explicitly via `make lint-cpp`, does not currently catch a lot of rules that are
specific to the Node.js C++ code base. This document explains the most common of
these rules:

## Left-leaning (C++ style) asterisks for pointer declarations

`char* buffer;` instead of `char *buffer;`

## 2 spaces of indentation for blocks or bodies of conditionals

```c++
if (foo)
  bar();
```

or

```c++
if (foo) {
  bar();
  baz();
}
```

Braces are optional if the statement body only has one line.

`namespace`s receive no indentation on their own.

## 4 spaces of indentation for statement continuations

```c++
VeryLongTypeName very_long_result = SomeValueWithAVeryLongName +
    SomeOtherValueWithAVeryLongName;
```

Operators are before the line break in these cases.

## Align function arguments vertically

```c++
void FunctionWithAVeryLongName(int parameter_with_a_very_long_name,
                               double other_parameter_with_a_very_long_name,
                               ...);
```

If that doesn’t work, break after the `(` and use 4 spaces of indentation:

```c++
void FunctionWithAReallyReallyReallyLongNameSeriouslyStopIt(
    int okay_there_is_no_space_left_in_the_previous_line,
    ...);
```

## Initialization lists

Long initialization lists are formatted like this:

```c++
HandleWrap::HandleWrap(Environment* env,
                       Local<Object> object,
                       uv_handle_t* handle,
                       AsyncWrap::ProviderType provider)
    : AsyncWrap(env, object, provider),
      state_(kInitialized),
      handle_(handle) {
```

## CamelCase for methods, functions and classes

Exceptions are simple getters/setters, which are named `property_name()` and
`set_property_name()`, respectively.

```c++
class FooBar {
 public:
  void DoSomething();
  static void DoSomethingButItsStaticInstead();

  void set_foo_flag(int flag_value);
  int foo_flag() const;  // Use const-correctness whenever possible.
};
```

## snake\_case for local variables and parameters

```c++
int FunctionThatDoesSomething(const char* important_string) {
  const char* pointer_into_string = important_string;
}
```

## snake\_case\_ for private class fields

```c++
class Foo {
 private:
  int counter_ = 0;
};
```

## Space after `template`

```c++
template <typename T>
class FancyContainer {
 ...
}
```

## Type casting

- Always avoid C-style casts (`(type)value`)
- `dynamic_cast` does not work because RTTI is not enabled
- Use `static_cast` for casting whenever it works
- `reinterpret_cast` is okay if `static_cast` is not appropriate

## Memory allocation

- `Malloc()`, `Calloc()`, etc. from `util.h` abort in Out-of-Memory situations
- `UncheckedMalloc()`, etc. return `nullptr` in OOM situations

## `nullptr` instead of `NULL` or `0`

What it says in the title.

## Do not include `*.h` if `*-inl.h` has already been included

Do

```cpp
#include "util-inl.h"  // already includes util.h
```

instead of

```cpp
#include "util.h"
#include "util-inl.h"
```

## Avoid throwing JavaScript errors in nested C++ methods

If you need to throw JavaScript errors from a C++ binding method, try to do it
at the top level and not inside of nested calls.

A lot of code inside Node.js is written so that typechecking etc. is performed
in JavaScript.

Using C++ `throw` is not allowed.
