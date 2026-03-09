# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.4](https://github.com/rust-lang/cfg-if/compare/v1.0.3...v1.0.4) - 2025-10-15

- Support `cfg(true)` and `cfg(false)` ([#99](https://github.com/rust-lang/cfg-if/pull/99))
- Set and test a MSRV of 1.32
- Have a single top-level rule

## [1.0.3](https://github.com/rust-lang/cfg-if/compare/v1.0.2...v1.0.3) - 2025-08-19

- Revert "Remove `@__identity` rule."

## [1.0.2](https://github.com/rust-lang/cfg-if/compare/v1.0.1...v1.0.2) - 2025-08-19

- Remove `@__identity` rule.

## [1.0.1](https://github.com/rust-lang/cfg-if/compare/v1.0.0...v1.0.1) - 2025-06-09

- Remove `compiler-builtins` from `rustc-dep-of-std` dependencies
- Remove redundant configuration from Cargo.toml
- More readable formatting and identifier names. ([#39](https://github.com/rust-lang/cfg-if/pull/39))
- Add expanded example to readme ([#38](https://github.com/rust-lang/cfg-if/pull/38))
