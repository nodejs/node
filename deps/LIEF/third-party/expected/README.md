# expected
Single header implementation of `std::expected` with functional-style extensions.

[![Documentation Status](https://readthedocs.org/projects/tl-docs/badge/?version=latest)](https://tl.tartanllama.xyz/en/latest/?badge=latest)
Clang + GCC: [![Linux Build Status](https://github.com/TartanLlama/expected/actions/workflows/cmake.yml/badge.svg)](https://github.com/TartanLlama/expected/actions/workflows/cmake.yml)
MSVC: [![Windows Build Status](https://ci.appveyor.com/api/projects/status/k5x00xa11y3s5wsg?svg=true)](https://ci.appveyor.com/project/TartanLlama/expected)

Available on [Vcpkg](https://github.com/microsoft/vcpkg/tree/master/ports/tl-expected), [Conan](https://github.com/yipdw/conan-tl-expected) and [`build2`](https://cppget.org/tl-expected).

[`std::expected`](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0323r3.pdf) is proposed as the preferred way to represent object which will either have an expected value, or an unexpected value giving information about why something failed. Unfortunately, chaining together many computations which may fail can be verbose, as error-checking code will be mixed in with the actual programming logic. This implementation provides a number of utilities to make coding with `expected` cleaner.

For example, instead of writing this code:

```cpp
std::expected<image,fail_reason> get_cute_cat (const image& img) {
    auto cropped = crop_to_cat(img);
    if (!cropped) {
      return cropped;
    }

    auto with_tie = add_bow_tie(*cropped);
    if (!with_tie) {
      return with_tie;
    }

    auto with_sparkles = make_eyes_sparkle(*with_tie);
    if (!with_sparkles) {
       return with_sparkles;
    }

    return add_rainbow(make_smaller(*with_sparkles));
}
```

You can do this:

```cpp
tl::expected<image,fail_reason> get_cute_cat (const image& img) {
    return crop_to_cat(img)
           .and_then(add_bow_tie)
           .and_then(make_eyes_sparkle)
           .map(make_smaller)
           .map(add_rainbow);
}
```

The interface is the same as `std::expected` as proposed in [p0323r3](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0323r3.pdf), but the following member functions are also defined. Explicit types are for clarity.

- `map`: carries out some operation on the stored object if there is one.
  * `tl::expected<std::size_t,std::error_code> s = exp_string.map(&std::string::size);`
- `map_error`: carries out some operation on the unexpected object if there is one.
  * `my_error_code translate_error (std::error_code);`
  * `tl::expected<int,my_error_code> s = exp_int.map_error(translate_error);`
- `and_then`: like `map`, but for operations which return a `tl::expected`.
  * `tl::expected<ast, fail_reason> parse (const std::string& s);`
  * `tl::expected<ast, fail_reason> exp_ast = exp_string.and_then(parse);`
- `or_else`: calls some function if there is no value stored.
  * `exp.or_else([] { throw std::runtime_error{"oh no"}; });`

p0323r3 specifies calling `.error()` on an expected value, or using the `*` or `->` operators on an unexpected value, to be undefined behaviour. In this implementation it causes an assertion failure. The implementation of assertions can be overridden by defining the macro `TL_ASSERT(boolean_condition)` before #including <tl/expected.hpp>; by default, `assert(boolean_condition)` from the `<cassert>` header is used. Note that correct code would not rely on these assertions.

### Compiler support

Tested on:

- Linux
  * clang++ 3.5, 3.6, 3.7, 3.8, 3.9, 4, 5, 6, 7, 8, 9, 10, 11
  * g++ 4.8, 4.9, 5.5, 6.4, 7.5, 8, 9, 10 
- Windows
  * MSVC 2015, 2017, 2019, 2022

----------

[![CC0](http://i.creativecommons.org/p/zero/1.0/88x31.png)]("http://creativecommons.org/publicdomain/zero/1.0/")

To the extent possible under law, [Sy Brand](https://twitter.com/TartanLlama) has waived all copyright and related or neighboring rights to the `expected` library. This work is published from: United Kingdom.
