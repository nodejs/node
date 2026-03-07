# if_chain

[![CI](https://github.com/lambda-fairy/if_chain/actions/workflows/ci.yml/badge.svg)](https://github.com/lambda-fairy/if_chain/actions/workflows/ci.yml) [![Cargo](https://img.shields.io/crates/v/if_chain.svg)](https://crates.io/crates/if_chain)

**If you're using Rust 1.88 or newer, check out [`if let` chains](https://blog.rust-lang.org/2025/06/26/Rust-1.88.0/#let-chains) instead. This crate is still available for earlier versions of Rust.**

This crate provides a single macro called `if_chain!`.

`if_chain!` lets you write long chains of nested `if` and `if let` statements without the associated rightward drift. It also supports multiple patterns (e.g. `if let Foo(a) | Bar(a) = b`) in places where Rust would normally not allow them.

For more information on this crate, see the [documentation](https://docs.rs/if_chain) and associated [blog post](https://lambda.xyz/blog/if-chain).
