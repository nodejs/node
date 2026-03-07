# Bit-Slice Rendering

This implementation prints the contents of a `&BitSlice` in one of binary,
octal, or hexadecimal. It is important to note that this does *not* render the
raw underlying memory! They render the semantically-ordered contents of the
bit-slice as numerals. This distinction matters if you use type parameters that
differ from those presumed by your debugger (which is usually `<u8, Msb0>`).

The output separates the `T` elements as individual list items, and renders each
element as a base- 2, 8, or 16 numeric string. When walking an element, the bits
traversed by the bit-slice are considered to be stored in
most-significant-bit-first ordering. This means that index `[0]` is the high bit
of the left-most digit, and index `[n]` is the low bit of the right-most digit,
in a given printed word.

In order to render according to expectations of the Arabic numeral system, an
element being transcribed is chunked into digits from the least-significant end
of its rendered form. This is most noticeable in octal, which will always have a
smaller ceiling on the left-most digit in a printed word, while the right-most
digit in that word is able to use the full `0 ..= 7` numeral range.

## Examples

```rust
# #[cfg(feature = "std")] {
use bitvec::prelude::*;

let data = [
  0b000000_10u8,
// digits print LTR
  0b10_001_101,
// significance is computed RTL
  0b01_000000,
];
let bits = &data.view_bits::<Msb0>()[6 .. 18];

assert_eq!(format!("{:b}", bits), "[10, 10001101, 01]");
assert_eq!(format!("{:o}", bits), "[2, 215, 1]");
assert_eq!(format!("{:X}", bits), "[2, 8D, 1]");
# }
```

The `{:#}` format modifier causes the standard `0b`, `0o`, or `0x` prefix to be
applied to each printed word. The other format specifiers are not interpreted by
this implementation, and apply to the entire rendered text, not to individual
words.
