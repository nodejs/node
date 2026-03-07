# Support for `serde`

`bitvec` structures are able to de/serialize their contents using `serde`.
Because `bitvec` is itself designed to be a transport buffer and have
memory-efficient storage properties, the implementations are somewhat strange
and not necessarily suitable for transport across heterogenous hosts.

`bitvec` always serializes its underlying memory storage as a sequence of raw
memory. It also includes the necessary metadata to prevent deserialization into
an incorrect type.

## Serialization

All data types serialize through `BitSlice`. While in version 0, `BitArray` had
its own serialization logic; this is no longer the case.

`BitSlice` serializes the bit-width of `T::Mem` and the `any::type_name` of `O`.
This may cause deserialization failures in the future if `any::type_name`
changes its behavior, but as it is run while compiling `bitvec` itself, clients
that rename `bitvec` when bringing it into their namespace should not be
affected.

Note that because `LocalBits` is a reëxport rather than a type in its own right,
it always serializes as the real type to which it forwards. This prevents
accidental mismatch when transporting between machines with different
destinations for this alias.

The next items serialized are the index of the starting bit within the starting
element, and the total number of bits included in the bit-slice. After these,
the data buffer is serialized directly.

Each element in the data buffer is loaded, has any dead bits cleared to `0`, and
is then serialized directly into the collector. In particular, no
byte-reördering for transport is performed here, so integers wider than a byte
must use a de/serialization format that handles this if, for example, byte-order
endian transforms are required.

## Deserialization

Serde only permits no-copy slice deserialization on `&'a [u8]` slices, so
`bitvec` in turn can only deserialize into `&'a BitSlice<u8, O>` bit-slices. It
can deserialize into `BitArray`s of any type, relying on the serialization layer
to reverse any byte-order transforms.

`&BitSlice` will only deserialize if the transport format contains the bytes
directly in it. If you do do not have an allocator, you should always transport
`BitArray`. If you do have an allocator, and are serializing `BitBox` or
`BitVec`, and expect to deserialize into a `BitArray`, then you will need to use
`.force_align()` and ensure that you fully occupy the buffer being transported.

`BitArray` will fail to deserialize if the data stream does not have a head-bit
index of `0` and a length that exactly matches its own. This limitation is a
consequence of the implementation, and likely will not be relaxed. `BitBox` and
`BitVec`, however, are able to deserialize any bit-sequence without issue.

## Warnings

`usize` *does* de/serialize! However, because it does not have a fixed width,
`bitvec` always serializes it as the local fixed-width equivalent, and places
the word width into the serialization stream. This will prevent roundtripping a
`BitArray<[usize; N]>` between hosts with different `usize` widths, even though
the types in the source code line up.

This behavior was not present in version 0, and users were able to write
programs that incorrectly handled de/serialization when used on heterogenous
systems.

In addition, remember that `bitvec` serializes its data buffer *directly* as
2’s-complement integers. You must ensure that your transport layer can handle
them correctly. As an example, JSON is not required to transport 64-bit integers
with perfect fidelity. `bitvec` has no way to detect inadequacy in the transport
layer, and will not prevent you from using a serialization format that damages
or alters the bit-stream you send through it.

## Transport Format

All `bitvec` data structures produce the same basic format: a structure (named
`BitSeq` for `BitSlice`, `BitBox`, and `BitVec`, or `BitArr` for `BitArray`)
with four fields:

1. `order` is a string naming the `O: BitOrder` parameter. Because it uses
   [`any::type_name`][0], its value cannot be *guaranteed* to be stable. You
   cannot assume that transport buffers are compatible across versions of the
   compiler used to create applications exchanging them.
1. `head` is a `BitIdx` structure containing two fields:
   1. `width` is a single byte containing `8`, `16`, `32`, or `64`, describing
      the bit-width of each element in the data buffer. `bitvec` structures will
      refuse to deserialize if the serialized bit-width does not match their
      `T::Mem` type.
   1. `index` is a single byte containing the head-bit that begins the live
      `BitSlice` region. `BitArray` will refuse to deserialize if this is not
      zero.
1. `bits` is the number of live bits in the region, as a `u64`. `BitArray` fails
   to deserialize if it does not match [`mem::bits_of::<A>()`][1].
1. `data` is the actual data buffer containing the bits being transported. For
   `BitSeq` serialization, it is a sequence; for `BitArr`, it is a tuple. This
   may affect the transport representation, and so the two are not guaranteed to
   be interchangeable over all transports.

As known examples, JSON does not have a fixed-size array type, so the contents
of all `bitvec` structures have identical rendering, while the [`bincode`] crate
does distinguish between run-length-encoded slices and non-length-encoded
arrays.

## Implementation Details

`bitvec` supports deserializing from both of Serde’s models for aggregate
structures: dictionaries and sequences. It always serializes as a dictionary,
but if your serialization layer does not want to include field names, you may
emit only the values *in the temporal order that they are received* and `bitvec`
will correctly deserialize from them.

`BitSlice` (and `BitBox` and `BitVec`, which forward to it) transports its data
buffer using Serde’s *sequence* model. `BitArray` uses Serde’s *tuple* model
instead. These models might not be interchangeable in certain transport formats!
You should always deserialize into the same container type that produced a
serialized stream.

[0]: core::any::type_name
[1]: crate::mem::bits_of
[`bincode`]: https://docs.rs/bincode/latest/bincode
