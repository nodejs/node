// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Getting a working keyboard when stdin is the data pipe.
//!
//! `d8 --print-maglev-graphs x.js | garage` is the primary invocation in
//! PLAN §9. In it stdin carries the trace, not keystrokes, so the keyboard has
//! to come from the controlling terminal instead. crossterm nominally handles
//! this — its unix event source opens `/dev/tty` when `isatty(0)` is false —
//! but on macOS that fallback does not work, and the failure is silent.
//!
//! ## Why `/dev/tty` is not enough on macOS
//!
//! crossterm's event source registers its input descriptor with `mio`, which is
//! `kqueue` on darwin. A descriptor obtained by opening `/dev/tty` cannot be
//! registered: `kevent` rejects it with `EINVAL`. Measured, in tmux:
//!
//! ```text
//! register(fd 0, a pty slave)      -> OK
//! register(open("/dev/tty"))       -> Err(EINVAL)      <-- crossterm's fallback
//! register(open("/dev/ttys003"))   -> OK               <-- the same terminal
//! ```
//!
//! `/dev/tty` is a clone device that resolves to the controlling terminal at
//! open time, and kqueue will not watch it. Note also that `ttyname()` on such
//! a descriptor answers `/dev/tty` rather than the underlying device, so the
//! name cannot be recovered from the descriptor itself. And `dup2`-ing it onto
//! fd 0 does not help: the rejection follows the open file description, not the
//! descriptor number.
//!
//! crossterm swallows the resulting error (`source.ok()`), so every later
//! `poll` returns "Failed to initialize input reader" and the UI simply never
//! receives a key. Worse, `crossterm::cursor::position` retries on error with
//! no backoff and no attempt limit, so anything that asks for the cursor
//! position — including `ratatui`'s `Terminal::clear` — spins at 100% CPU
//! forever.
//!
//! ## The fix
//!
//! `stdout` and `stderr` still point at the terminal even when stdin is a pipe,
//! and *their* descriptors refer to the pty slave directly. So: ask `ttyname`
//! for the real device path, open that, and `dup2` it onto fd 0. crossterm's
//! `isatty(0)` check then takes the path that works, with no patching of
//! crossterm required. The original stdin is duplicated out of the way first
//! and handed back for the reader thread to consume.
//!
//! This costs nothing on Linux, where `/dev/tty` would have worked anyway, and
//! keeps one code path rather than two.

use std::fs::File;
use std::io::{self, IsTerminal};

use anyhow::{Context, Result};

use crate::tracing;
use crate::tui::term;

/// Descriptors sorted out for a session.
pub struct TerminalAccess {
    /// The original stdin, when it was a pipe and had to be moved off fd 0.
    /// This — not `io::stdin()` — is where trace bytes come from afterwards.
    pub data: Option<File>,
}

/// Where the UI is drawn. Not necessarily stdout.
pub enum Screen {
    Stdout(io::Stdout),
    #[cfg(unix)]
    Tty(File),
}

impl Screen {
    /// Picks the drawing target: stdout when it is a terminal, else `/dev/tty`.
    ///
    /// `garage x.log > out.txt` must not fill `out.txt` with escape sequences.
    /// Opening `/dev/tty` is fine here — the kqueue restriction above applies
    /// to *watching* a descriptor, not to writing to one.
    pub fn open() -> Result<Self> {
        if io::stdout().is_terminal() {
            return Ok(Screen::Stdout(io::stdout()));
        }

        #[cfg(unix)]
        {
            let tty = File::options()
                .read(true)
                .write(true)
                .open("/dev/tty")
                .context(
                    "stdout is not a terminal and /dev/tty cannot be opened, \
                     so there is nowhere to draw",
                )?;
            tracing::debug!("stdout is redirected; drawing on /dev/tty");
            Ok(Screen::Tty(tty))
        }

        #[cfg(not(unix))]
        {
            anyhow::bail!("stdout is not a terminal");
        }
    }
}

impl io::Write for Screen {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        match self {
            Screen::Stdout(w) => w.write(buf),
            #[cfg(unix)]
            Screen::Tty(w) => w.write(buf),
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        match self {
            Screen::Stdout(w) => w.flush(),
            #[cfg(unix)]
            Screen::Tty(w) => w.flush(),
        }
    }
}

/// True when the trace has to come from stdin. Must be consulted *before*
/// [`acquire`], which deliberately makes stdin a terminal.
pub fn stdin_is_data() -> bool {
    !io::stdin().is_terminal()
}

/// Points fd 0 at the controlling terminal, returning the displaced stdin.
///
/// A no-op when stdin is already a terminal. Must run before anything touches
/// `crossterm::event`: crossterm builds its event source once, caches it, and
/// never retries, so a source built against the wrong descriptor stays broken
/// for the life of the process.
#[cfg(unix)]
pub fn acquire() -> Result<TerminalAccess> {
    use std::os::fd::{AsRawFd, FromRawFd};

    if io::stdin().is_terminal() {
        return Ok(TerminalAccess { data: None });
    }

    // Duplicate the pipe before fd 0 is repointed at the terminal.
    let saved = unsafe { libc::dup(libc::STDIN_FILENO) };
    if saved < 0 {
        return Err(io::Error::last_os_error()).context("cannot duplicate stdin");
    }
    // SAFETY: `dup` just returned this descriptor and nothing else owns it.
    let data = unsafe { File::from_raw_fd(saved) };

    let terminal = open_controlling_terminal()?;
    if unsafe { libc::dup2(terminal.as_raw_fd(), libc::STDIN_FILENO) } < 0 {
        return Err(io::Error::last_os_error()).context("cannot point stdin at the terminal");
    }
    // `terminal` closes here; fd 0 keeps the open file description alive.

    Ok(TerminalAccess { data: Some(data) })
}

#[cfg(not(unix))]
pub fn acquire() -> Result<TerminalAccess> {
    if io::stdin().is_terminal() {
        return Ok(TerminalAccess { data: None });
    }
    anyhow::bail!("reading a trace from a pipe needs a unix controlling terminal")
}

/// Opens the controlling terminal by its real device path.
///
/// Deliberately not `/dev/tty` first: see the module comment. `/dev/tty`
/// remains as a fallback for the case where neither stdout nor stderr is a
/// terminal, which on Linux still works.
#[cfg(unix)]
fn open_controlling_terminal() -> Result<File> {
    for fd in [libc::STDOUT_FILENO, libc::STDERR_FILENO] {
        let Some(path) = ttyname(fd) else { continue };
        match File::options().read(true).write(true).open(&path) {
            Ok(file) => {
                tracing::debug!(path = %path, "controlling terminal");
                return Ok(file);
            }
            Err(e) => tracing::warn!(path = %path, "cannot open terminal device: {e}"),
        }
    }

    File::options()
        .read(true)
        .write(true)
        .open("/dev/tty")
        .context(
            "stdin is a pipe, neither stdout nor stderr is a terminal, and \
             /dev/tty cannot be opened, so there is no keyboard to read",
        )
}

/// The device path behind a descriptor, if it is a terminal.
#[cfg(unix)]
fn ttyname(fd: i32) -> Option<String> {
    if unsafe { libc::isatty(fd) } != 1 {
        return None;
    }
    let name = unsafe { libc::ttyname(fd) };
    if name.is_null() {
        return None;
    }
    // SAFETY: ttyname returned a non-null pointer to a NUL-terminated string
    // in thread-local storage, which is read before any other ttyname call.
    let name = unsafe { std::ffi::CStr::from_ptr(name) };
    Some(name.to_string_lossy().into_owned())
}

/// Proves keystrokes can actually be read, and fails loudly if not.
///
/// crossterm reports a dead event source only as an error from `poll`, and
/// only after it has already been cached. A zero-length poll forces it to be
/// built now, so an unusable terminal is a startup error rather than a UI that
/// paints once and then ignores the keyboard forever.
pub fn verify_keyboard() -> Result<()> {
    term::event::poll(std::time::Duration::ZERO)
        .context("terminal input is unavailable (no controlling terminal?)")?;
    Ok(())
}
