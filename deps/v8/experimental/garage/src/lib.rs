// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `garage` — a terminal UI for V8 trace output.
//!
//! Split into a library and a thin binary so the parsers can be tested without
//! a terminal. The golden-file suite over `fixtures/` is the reason
//! this matters: those tests need the parsing half and none of the UI half.

pub mod ansi;
pub mod app;
pub mod cli;
pub mod clipboard;
pub mod config;
pub mod diff;
pub mod event;
pub mod index;
pub mod logging;
pub mod markers;
pub mod model;
pub mod myers;
pub mod parse;
pub mod source;
pub mod terminal;
pub mod toml_min;
pub mod tty;
pub mod tui;
pub mod ui;
pub mod update;
pub mod view;

pub use logging::tracing;
