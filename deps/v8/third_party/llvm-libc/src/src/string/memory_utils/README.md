# The mem* framework

The framework handles the following mem* functions:
 - `memcpy`
 - `memmove`
 - `memset`
 - `bzero`
 - `bcmp`
 - `memcmp`

## Building blocks

These functions can be built out of a set of lower-level operations:
 - **`block`** : operates on a block of `SIZE` bytes.
 - **`tail`** : operates on the last `SIZE` bytes of the buffer (e.g., `[dst + count - SIZE, dst + count]`)
 - **`head_tail`** : operates on the first and last `SIZE` bytes. This is the same as calling `block` and `tail`.
 - **`loop_and_tail`** : calls `block` in a loop to consume as much as possible of the `count` bytes and handle the remaining bytes with a `tail` operation.

As an illustration, let's take the example of a trivial `memset` implementation:

 ```C++
 extern "C" void memset(const char* dst, int value, size_t count) {
    if (count == 0) return;
    if (count == 1) return Memset<1>::block(dst, value);
    if (count == 2) return Memset<2>::block(dst, value);
    if (count == 3) return Memset<3>::block(dst, value);
    if (count <= 8) return Memset<4>::head_tail(dst, value, count);  // Note that 0 to 4 bytes are written twice.
    if (count <= 16) return Memset<8>::head_tail(dst, value, count); // Same here.
    return Memset<16>::loop_and_tail(dst, value, count);
}
 ```

Now let's have a look into the `Memset` structure:

```C++
template <size_t Size>
struct Memset {
  static constexpr size_t SIZE = Size;

  LIBC_INLINE static void block(Ptr dst, uint8_t value) {
    // Implement me
  }

  LIBC_INLINE static void tail(Ptr dst, uint8_t value, size_t count) {
    block(dst + count - SIZE, value);
  }

  LIBC_INLINE static void head_tail(Ptr dst, uint8_t value, size_t count) {
    block(dst, value);
    tail(dst, value, count);
  }

  LIBC_INLINE static void loop_and_tail(Ptr dst, uint8_t value, size_t count) {
    size_t offset = 0;
    do {
      block(dst + offset, value);
      offset += SIZE;
    } while (offset < count - SIZE);
    tail(dst, value, count);
  }
};
```

As you can see, the `tail`, `head_tail` and `loop_and_tail` are higher order functions that build on each others. Only `block` really needs to be implemented.
In earlier designs we were implementing these higher order functions with templated functions but it appears that it is more readable to have the implementation explicitly stated.
**This design is useful because it provides customization points**. For instance, for `bcmp` on `aarch64` we can provide a better implementation of `head_tail` using vector reduction intrinsics.

## Scoped specializations

We can have several specializations of the `Memset` structure. Depending on the target requirements we can use one or several scopes for the same implementation.

In the following example we use the `generic` implementation for the small sizes but use the `x86` implementation for the loop.
```C++
 extern "C" void memset(const char* dst, int value, size_t count) {
    if (count == 0) return;
    if (count == 1) return generic::Memset<1>::block(dst, value);
    if (count == 2) return generic::Memset<2>::block(dst, value);
    if (count == 3) return generic::Memset<3>::block(dst, value);
    if (count <= 8) return generic::Memset<4>::head_tail(dst, value, count);
    if (count <= 16) return generic::Memset<8>::head_tail(dst, value, count);
    return x86::Memset<16>::loop_and_tail(dst, value, count);
}
```

### The `builtin` scope

Ultimately we would like the compiler to provide the code for the `block` function. For this we rely on dedicated builtins available in Clang (e.g., [`__builtin_memset_inline`](https://clang.llvm.org/docs/LanguageExtensions.html#guaranteed-inlined-memset))

### The `generic` scope

In this scope we define pure C++ implementations using native integral types and clang vector extensions.

### The arch specific scopes

Then comes implementations that are using specific architectures or microarchitectures features (e.g., `rep;movsb` for `x86` or `dc zva` for `aarch64`).

The purpose here is to rely on builtins as much as possible and fallback to `asm volatile` as a last resort.
