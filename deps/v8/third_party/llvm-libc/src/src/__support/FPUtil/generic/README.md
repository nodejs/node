This directory contains machine independent implementations of floating point
operations. The implementations are nested in the namespace
`LIBC_NAMESPACE::fputil::generic`. This is to facilitate calling these generic
implementations from machine dependent implementations. Consider the example of
the fuse-multiply-add operation (FMA). The C standard library requires three
different flavors, `fma` which operates double precision numbers, `fmaf` which
operates on single precision numbers, and `fmal` which operates on `long double`
numbers. On aarch64, there are hardware instructions which implement the single
and double precision flavors but not the `long double` flavor. For such targets,
we want to be able to call the generic `long double` implementation from the
`long double` flavor. By putting the generic implementations in a separate
nested namespace, we will be to call them as follows:

```
namespace LIBC_NAMESPACE_DECL {
namespace fputil {

long double fmal(long double x, long double y, long double z) {
  return generic::fmal(x, y, z);
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL
```

Note that actual code might not be as straightforward as above (for example,
we might want to prevent implicit type promotions by using some template
facilities). But, the general idea is very similar.
