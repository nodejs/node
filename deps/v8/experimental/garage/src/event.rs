// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The single event stream the UI thread blocks on.
//!
//! Redraw is event-driven (PLAN §11): the main loop blocks on `recv`, so an
//! idle `garage` uses no CPU. There is no 60 fps tick. Everything that can
//! cause a redraw — keys, resize, new trace bytes — arrives here.

use std::sync::mpsc::Sender;
use std::thread;

use crate::source::SourceEvent;
use crate::tracing;
use crate::tui::term;

pub enum Event {
    /// A key, resize, or other terminal event.
    Input(term::event::Event),
    /// Data or lifecycle from a reader thread.
    Source(SourceEvent),
    /// The input thread stopped, which means the terminal went away.
    InputClosed,
}

/// Starts the thread that turns terminal events into `Event::Input`.
///
/// Separate from the readers so that a blocked or slow source can never make
/// the UI stop responding to keys — including `q`.
pub fn spawn_input_thread(tx: Sender<Event>) -> std::io::Result<()> {
    thread::Builder::new()
        .name("garage-input".to_string())
        .spawn(move || {
            loop {
                match term::event::read() {
                    Ok(ev) => {
                        if tx.send(Event::Input(ev)).is_err() {
                            return; // UI gone
                        }
                    }
                    Err(e) => {
                        tracing::error!("terminal input failed: {e}");
                        let _ = tx.send(Event::InputClosed);
                        return;
                    }
                }
            }
        })
        .map(|_| ())
}
