# Anti-Aliasing Iterator Adapter

This structure is an adapter over a corresponding `&mut BitSlice` iterator. It
removes the `::Alias` taint marker, allowing mutations through each yielded bit
reference to skip any costs associated with aliasing.

## Safety

The default `&mut BitSlice` iterators attach an `::Alias` taint for a reason:
the iterator protocol does not mandate that yielded items have a narrower
lifespan than the iterator that produced them! As such, it is completely
possible to pull multiple yielded items out into the same scope, where they have
overlapping lifetimes.

The `BitStore` principles require that whenever two write-capable handles to the
same memory region have overlapping lifetimes, they *must* be `::Alias` tainted.
This adapter removes the `::Alias` taint, but is not able to enforce strictly
non-overlapping lifetimes of yielded items.

As such, this adapter is **unsafe to construct**, and you **must** only use it
in a `for`-loop where each yielded item does not escape the loop body.

In order to help enforce this limitation, this adapter structure is *not* `Send`
or `Sync`. It must be consumed in the scope where it was created.

## Usage

If you are using a loop that satisfies the safety requirement, you can use the
`.remove_alias()` method on your mutable iterator and configure it to yield
handles that do not impose additional alias-protection costs when accessing the
underlying memory.

Note that this adapter does not go to `T::Unalias`: it only takes an iterator
that yields `T::Alias` and unwinds it to `T`. If the source bit-slice was
*already* alias-tainted, the original protection is not removed. You are
responsible for doing so by using [`.bit_domain_mut()`].

## Examples

This example shows using `.chunks_mut()` without incurring alias protection.

This documentation is replicated on all `NoAlias` types; the examples will work
for all of them, but are not specialized in the text.

```rust
use bitvec::prelude::*;
use bitvec::slice::{ChunksMut, ChunksMutNoAlias};
type Alias8 = <u8 as BitStore>::Alias;

let mut data: BitArr!(for 40, in u8, Msb0) = bitarr![u8, Msb0; 0; 40];

let mut chunks: ChunksMut<u8, Msb0> = data.chunks_mut(5);
let _chunk: &mut BitSlice<Alias8, Msb0> = chunks.next().unwrap();

let mut chunks: ChunksMutNoAlias<u8, Msb0> = unsafe { chunks.remove_alias() };
let _chunk: &mut BitSlice<u8, Msb0> = chunks.next().unwrap();
```

This example shows how use of [`.split_at_mut()`] forces the `.remove_alias()` to
still retain a layer of alias protection.

```rust
use bitvec::prelude::*;
use bitvec::slice::{ChunksMut, ChunksMutNoAlias};
type Alias8 = <u8 as BitStore>::Alias;
type Alias8Alias = <Alias8 as BitStore>::Alias;

let mut data: BitArr!(for 40, in u8, Msb0) = bitarr!(u8, Msb0; 0; 40);
let (_head, rest): (_, &mut BitSlice<Alias8, Msb0>) = data.split_at_mut(5);

let mut chunks: ChunksMut<Alias8, Msb0> = rest.chunks_mut(5);
let _chunk: &mut BitSlice<Alias8, Msb0> = chunks.next().unwrap();

let mut chunks: ChunksMutNoAlias<Alias8, Msb0> = unsafe { chunks.remove_alias() };
let _chunk: &mut BitSlice<Alias8, Msb0> = chunks.next().unwrap();
```

And this example shows how to use `.bit_domain_mut()` in order to undo the
effects of `.split_at_mut()`, so that `.remove_alias()` can complete its work.

```rust
use bitvec::prelude::*;
use bitvec::slice::{ChunksMut, ChunksMutNoAlias};
type Alias8 = <u8 as BitStore>::Alias;

let mut data: BitArr!(for 40, in u8, Msb0) = bitarr!(u8, Msb0; 0; 40);
let (_head, rest): (_, &mut BitSlice<Alias8, Msb0>) = data.split_at_mut(5);

let (head, body, tail): (
  &mut BitSlice<Alias8, Msb0>,
  &mut BitSlice<u8, Msb0>,
  &mut BitSlice<Alias8, Msb0>,
) = rest.bit_domain_mut().region().unwrap();

let mut chunks: ChunksMut<u8, Msb0> = body.chunks_mut(5);
let _chunk: &mut BitSlice<Alias8, Msb0> = chunks.next().unwrap();

let mut chunks: ChunksMutNoAlias<u8, Msb0> = unsafe { chunks.remove_alias() };
let _chunk: &mut BitSlice<u8, Msb0> = chunks.next().unwrap();
```

[`.bit_domain_mut()`]: crate::slice::BitSlice::bit_domain_mut
[`.split_at_mut()`]: crate::slice::BitSlice::split_at_mut
