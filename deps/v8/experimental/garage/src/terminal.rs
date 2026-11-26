// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Terminal lifecycle.
//!
//! Raw mode and the alternate screen are process-global side effects on the
//! user's terminal. Leaving them set is the classic way a TUI ruins a shell
//! session, so restoration has to survive every exit path — clean return,
//! error return, and panic.
//!
//! Drop alone is not enough for the panic case: the panic hook runs *before*
//! unwinding, so without a hook the panic message is printed into the alternate
//! screen and then vanishes with it. Hence a hook that restores first and then
//! delegates to the default one, plus an idempotent `restore` so the guard's
//! Drop during unwinding is a no-op.

use std::io::Write;
use std::sync::Once;
use std::sync::atomic::{AtomicBool, Ordering};

use crate::tui::render::Terminal;
use crate::tui::render::backend::AnsiBackend;
use crate::tui::term::event::{DisableMouseCapture, EnableMouseCapture};
use crate::tui::term::{cursor, execute_commands, terminal};
use anyhow::{Context, Result};

use crate::tty::Screen;

pub type Tui = Terminal<AnsiBackend<Screen>>;

/// True whenever the terminal is in its normal state, so `restore` can be
/// called from anywhere as often as it likes.
static RESTORED: AtomicBool = AtomicBool::new(true);
static PANIC_HOOK: Once = Once::new();

/// Owns the terminal's modified state. Dropping it puts the terminal back.
pub struct TerminalGuard {
    tui: Tui,
}

impl TerminalGuard {
    pub fn tui(&mut self) -> &mut Tui {
        &mut self.tui
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        restore();
    }
}

/// Enters raw mode and the alternate screen, with mouse capture.
///
/// Mouse capture takes over the terminal's own selection; the keyboard-only
/// stance Phase 1 took traded scroll-wheel support for native copy-paste.
/// With `y`/`Y`/`E` covering copying, the wheel and click-to-place-cursor won
/// (and terminals still offer native selection behind Shift- or Option-drag).
pub fn enter() -> Result<TerminalGuard> {
    crate::tty::verify_keyboard()?;

    // Before raw mode: the signal handler restores the *original* termios.
    install_signal_handlers();

    let mut screen = Screen::open()?;
    terminal::enable_raw_mode().context("cannot enable raw mode")?;
    RESTORED.store(false, Ordering::SeqCst);

    // From here on any early return must restore, which the guard below cannot
    // yet do — so failures go through `restore`.
    if let Err(e) = execute_commands(
        &mut screen,
        &[
            &terminal::EnterAlternateScreen,
            &cursor::Hide,
            &EnableMouseCapture,
        ],
    ) {
        restore();
        return Err(e).context("cannot enter the alternate screen");
    }

    install_panic_hook();

    // Deliberately no `Terminal::clear()` here. It snapshots the cursor
    // position first, and crossterm implements that as a DSR round-trip
    // (`ESC[6n` to stdout, answer back through the event source) with a retry
    // loop that has neither backoff nor an attempt limit. Two ways that hangs
    // at 100% CPU: an event source that failed to initialise, and a redirected
    // stdout, which sends the query into a file where no terminal will ever
    // answer it. Entering the alternate screen already gives a blank screen,
    // and the first `draw` paints all of it, so the round-trip buys nothing.
    let tui = match Terminal::new(AnsiBackend::new(screen)) {
        Ok(t) => t,
        Err(e) => {
            restore();
            return Err(e).context("cannot initialise the terminal backend");
        }
    };

    Ok(TerminalGuard { tui })
}

/// The termios captured before raw mode — what the signal handler puts
/// back. `tcsetattr` is async-signal-safe; crossterm's restore is not.
static SAVED_TERMIOS: std::sync::OnceLock<libc::termios> = std::sync::OnceLock::new();

/// SIGINT / SIGTERM / SIGHUP: take the wrapped child down, put
/// the terminal back, and die with the conventional code. Only
/// async-signal-safe calls: kill(2), tcsetattr(3), write(2), _exit(2).
/// SIGINT can only arrive from outside (`kill -INT`): raw mode disables
/// ISIG, so Ctrl+C is an ordinary key event.
fn install_signal_handlers() {
    let mut t = unsafe { std::mem::zeroed::<libc::termios>() };
    if unsafe { libc::tcgetattr(0, &mut t) } == 0 {
        let _ = SAVED_TERMIOS.set(t);
    }
    let handler = on_signal as extern "C" fn(libc::c_int) as *const () as libc::sighandler_t;
    unsafe {
        libc::signal(libc::SIGTERM, handler);
        libc::signal(libc::SIGINT, handler);
        libc::signal(libc::SIGHUP, handler);
    }
}

extern "C" fn on_signal(sig: libc::c_int) {
    crate::source::kill_child();
    if let Some(t) = SAVED_TERMIOS.get() {
        unsafe {
            libc::tcsetattr(0, libc::TCSANOW, t);
        }
    }
    // Release the mouse, leave the alternate screen, show the cursor.
    const RESTORE: &[u8] = b"\x1b[?1006l\x1b[?1002l\x1b[?1000l\x1b[?1049l\x1b[?25h";
    unsafe {
        libc::write(1, RESTORE.as_ptr().cast(), RESTORE.len());
        libc::_exit(128 + sig);
    }
}

/// Puts the terminal back. Idempotent, best-effort, safe to call from a panic
/// hook or a signal-ish context.
pub fn restore() {
    if RESTORED.swap(true, Ordering::SeqCst) {
        return;
    }

    let _ = terminal::disable_raw_mode();

    // A fresh handle rather than the terminal's own: the panic hook does not
    // have access to it, and this way there is exactly one restore path.
    if let Ok(mut screen) = Screen::open() {
        let _ = execute_commands(
            &mut screen,
            &[
                &DisableMouseCapture,
                &terminal::LeaveAlternateScreen,
                &cursor::Show,
            ],
        );
        let _ = screen.flush();
    }
}

fn install_panic_hook() {
    PANIC_HOOK.call_once(|| {
        let previous = std::panic::take_hook();
        std::panic::set_hook(Box::new(move |info| {
            // Order matters: restore first so the panic message lands on the
            // user's real screen and stays there.
            restore();
            crate::logging::record_panic(info);
            previous(info);
        }));
    });
}
