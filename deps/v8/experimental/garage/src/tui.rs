// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Self-contained POSIX terminal backend and cell-diffing TUI renderer.

pub mod term {
    use std::io::{self, Write};
    use std::ops::{BitOr, BitOrAssign, Sub, SubAssign};
    use std::sync::atomic::{AtomicI32, Ordering};
    use std::sync::{Mutex, OnceLock};
    use std::time::{Duration, Instant};

    pub trait Command {
        fn write_ansi(&self, f: &mut Vec<u8>);
    }

    pub fn execute_commands<W: Write>(w: &mut W, cmds: &[&dyn Command]) -> io::Result<()> {
        let mut buf = Vec::with_capacity(64);
        for cmd in cmds {
            cmd.write_ansi(&mut buf);
        }
        w.write_all(&buf)?;
        w.flush()
    }

    pub mod cursor {
        use super::Command;

        pub struct Hide;
        impl Command for Hide {
            fn write_ansi(&self, f: &mut Vec<u8>) {
                f.extend_from_slice(b"\x1b[?25l");
            }
        }

        pub struct Show;
        impl Command for Show {
            fn write_ansi(&self, f: &mut Vec<u8>) {
                f.extend_from_slice(b"\x1b[?25h");
            }
        }
    }

    pub mod terminal {
        use std::io;
        use std::sync::Mutex;

        use super::Command;

        pub struct EnterAlternateScreen;
        impl Command for EnterAlternateScreen {
            fn write_ansi(&self, f: &mut Vec<u8>) {
                f.extend_from_slice(b"\x1b[?1049h");
            }
        }

        pub struct LeaveAlternateScreen;
        impl Command for LeaveAlternateScreen {
            fn write_ansi(&self, f: &mut Vec<u8>) {
                f.extend_from_slice(b"\x1b[?1049l");
            }
        }

        #[cfg(unix)]
        static ORIG_TERMIOS: Mutex<Option<libc::termios>> = Mutex::new(None);

        #[cfg(unix)]
        pub fn enable_raw_mode() -> io::Result<()> {
            let mut guard = ORIG_TERMIOS.lock().unwrap_or_else(|e| e.into_inner());
            let mut orig = unsafe { std::mem::zeroed::<libc::termios>() };
            if unsafe { libc::tcgetattr(libc::STDIN_FILENO, &mut orig) } != 0 {
                return Err(io::Error::last_os_error());
            }
            if guard.is_none() {
                *guard = Some(orig);
            }
            let mut raw = orig;
            unsafe {
                libc::cfmakeraw(&mut raw);
            }
            raw.c_cc[libc::VMIN] = 1;
            raw.c_cc[libc::VTIME] = 0;
            if unsafe { libc::tcsetattr(libc::STDIN_FILENO, libc::TCSANOW, &raw) } != 0 {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        }

        #[cfg(not(unix))]
        pub fn enable_raw_mode() -> io::Result<()> {
            Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "raw mode requires unix",
            ))
        }

        #[cfg(unix)]
        pub fn disable_raw_mode() -> io::Result<()> {
            let mut guard = ORIG_TERMIOS.lock().unwrap_or_else(|e| e.into_inner());
            if let Some(orig) = guard.take()
                && unsafe { libc::tcsetattr(libc::STDIN_FILENO, libc::TCSANOW, &orig) } != 0
            {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        }

        #[cfg(not(unix))]
        pub fn disable_raw_mode() -> io::Result<()> {
            Ok(())
        }

        #[cfg(unix)]
        pub fn size() -> io::Result<(u16, u16)> {
            for fd in [libc::STDIN_FILENO, libc::STDOUT_FILENO, libc::STDERR_FILENO] {
                if let Some(sz) = winsize_of(fd) {
                    return Ok(sz);
                }
            }
            if let Ok(tty) = std::fs::File::open("/dev/tty") {
                use std::os::fd::AsRawFd;
                if let Some(sz) = winsize_of(tty.as_raw_fd()) {
                    return Ok(sz);
                }
            }
            Ok((80, 24))
        }

        #[cfg(unix)]
        fn winsize_of(fd: i32) -> Option<(u16, u16)> {
            let mut ws = unsafe { std::mem::zeroed::<libc::winsize>() };
            if unsafe { libc::ioctl(fd, libc::TIOCGWINSZ, &mut ws) } == 0
                && ws.ws_col > 0
                && ws.ws_row > 0
            {
                Some((ws.ws_col, ws.ws_row))
            } else {
                None
            }
        }

        #[cfg(not(unix))]
        pub fn size() -> io::Result<(u16, u16)> {
            Ok((80, 24))
        }
    }

    pub mod event {
        use super::*;

        pub struct EnableMouseCapture;
        impl Command for EnableMouseCapture {
            fn write_ansi(&self, f: &mut Vec<u8>) {
                f.extend_from_slice(b"\x1b[?1000h\x1b[?1002h\x1b[?1006h");
            }
        }

        pub struct DisableMouseCapture;
        impl Command for DisableMouseCapture {
            fn write_ansi(&self, f: &mut Vec<u8>) {
                f.extend_from_slice(b"\x1b[?1006l\x1b[?1002l\x1b[?1000l");
            }
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
        pub struct KeyModifiers(u8);

        impl KeyModifiers {
            pub const NONE: Self = Self(0);
            pub const SHIFT: Self = Self(1 << 0);
            pub const CONTROL: Self = Self(1 << 1);
            pub const ALT: Self = Self(1 << 2);

            pub const fn contains(self, other: Self) -> bool {
                (self.0 & other.0) == other.0
            }
        }

        impl BitOr for KeyModifiers {
            type Output = Self;
            fn bitor(self, rhs: Self) -> Self {
                Self(self.0 | rhs.0)
            }
        }

        impl BitOrAssign for KeyModifiers {
            fn bitor_assign(&mut self, rhs: Self) {
                self.0 |= rhs.0;
            }
        }

        impl Sub for KeyModifiers {
            type Output = Self;
            fn sub(self, rhs: Self) -> Self {
                Self(self.0 & !rhs.0)
            }
        }

        impl SubAssign for KeyModifiers {
            fn sub_assign(&mut self, rhs: Self) {
                self.0 &= !rhs.0;
            }
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub enum KeyCode {
            Backspace,
            Enter,
            Left,
            Right,
            Up,
            Down,
            Home,
            End,
            PageUp,
            PageDown,
            Tab,
            BackTab,
            Delete,
            Insert,
            F(u8),
            Char(char),
            Null,
            Esc,
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub enum KeyEventKind {
            Press,
            Repeat,
            Release,
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub struct KeyEvent {
            pub code: KeyCode,
            pub modifiers: KeyModifiers,
            pub kind: KeyEventKind,
        }

        impl KeyEvent {
            pub const fn new(code: KeyCode, modifiers: KeyModifiers) -> Self {
                Self {
                    code,
                    modifiers,
                    kind: KeyEventKind::Press,
                }
            }
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub enum MouseButton {
            Left,
            Right,
            Middle,
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub enum MouseEventKind {
            Down(MouseButton),
            Up(MouseButton),
            Drag(MouseButton),
            Moved,
            ScrollDown,
            ScrollUp,
            ScrollLeft,
            ScrollRight,
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub struct MouseEvent {
            pub kind: MouseEventKind,
            pub column: u16,
            pub row: u16,
            pub modifiers: KeyModifiers,
        }

        #[derive(Debug, Clone, PartialEq, Eq)]
        pub enum Event {
            Key(KeyEvent),
            Mouse(MouseEvent),
            Resize(u16, u16),
        }

        #[cfg(unix)]
        static WINCH_WRITE_FD: AtomicI32 = AtomicI32::new(-1);

        #[cfg(unix)]
        extern "C" fn on_sigwinch(_sig: libc::c_int) {
            let fd = WINCH_WRITE_FD.load(Ordering::Relaxed);
            if fd >= 0 {
                let byte: u8 = 1;
                unsafe {
                    let _ = libc::write(fd, (&raw const byte).cast(), 1);
                }
            }
        }

        #[cfg(unix)]
        fn winch_read_fd() -> Option<i32> {
            static WINCH_READ_FD: OnceLock<Option<i32>> = OnceLock::new();
            *WINCH_READ_FD.get_or_init(|| {
                let mut fds = [0i32; 2];
                if unsafe { libc::pipe(fds.as_mut_ptr()) } != 0 {
                    return None;
                }
                for &fd in &fds {
                    unsafe {
                        let fl = libc::fcntl(fd, libc::F_GETFD);
                        if fl >= 0 {
                            let _ = libc::fcntl(fd, libc::F_SETFD, fl | libc::FD_CLOEXEC);
                        }
                        let st = libc::fcntl(fd, libc::F_GETFL);
                        if st >= 0 {
                            let _ = libc::fcntl(fd, libc::F_SETFL, st | libc::O_NONBLOCK);
                        }
                    }
                }
                WINCH_WRITE_FD.store(fds[1], Ordering::SeqCst);
                let handler =
                    on_sigwinch as extern "C" fn(libc::c_int) as *const () as libc::sighandler_t;
                unsafe {
                    libc::signal(libc::SIGWINCH, handler);
                }
                Some(fds[0])
            })
        }

        pub(crate) struct ReaderState {
            buf: Vec<u8>,
            queued: std::collections::VecDeque<Event>,
        }

        impl ReaderState {
            pub(crate) fn new() -> Self {
                Self {
                    buf: Vec::with_capacity(256),
                    queued: std::collections::VecDeque::new(),
                }
            }
        }

        static READER: OnceLock<Mutex<ReaderState>> = OnceLock::new();

        fn state() -> &'static Mutex<ReaderState> {
            READER.get_or_init(|| Mutex::new(ReaderState::new()))
        }

        #[cfg(unix)]
        pub fn poll(timeout: Duration) -> io::Result<bool> {
            if unsafe { libc::isatty(libc::STDIN_FILENO) } != 1 {
                return Err(io::Error::new(
                    io::ErrorKind::NotConnected,
                    "stdin is not a terminal",
                ));
            }
            // On macOS (XNU), character clone devices like `/dev/tty` cannot be
            // watched by kqueue (which `tools/check-terminal.py` explicitly
            // tests for). Probe kqueue on macOS so `/dev/tty` fails promptly
            // with a clean error instead of silently diverging from the
            // documented Darwin contract.
            #[cfg(target_os = "macos")]
            {
                check_darwin_kqueue(libc::STDIN_FILENO)?;
            }

            let guard = state().lock().unwrap_or_else(|e| e.into_inner());
            if !guard.queued.is_empty() || !guard.buf.is_empty() {
                return Ok(true);
            }
            drop(guard);

            poll_fds(libc::STDIN_FILENO, Some(timeout))
                .map(|(stdin_ready, winch_ready)| stdin_ready || winch_ready)
        }

        #[cfg(target_os = "macos")]
        fn check_darwin_kqueue(fd: i32) -> io::Result<()> {
            let kq = unsafe { libc::kqueue() };
            if kq < 0 {
                return Err(io::Error::last_os_error());
            }
            let ev = libc::kevent {
                ident: fd as usize,
                filter: libc::EVFILT_READ,
                flags: libc::EV_ADD | libc::EV_RECEIPT,
                fflags: 0,
                data: 0,
                udata: std::ptr::null_mut(),
            };
            let mut out = ev;
            let rc = unsafe { libc::kevent(kq, &ev, 1, &mut out, 1, std::ptr::null()) };
            unsafe {
                libc::close(kq);
            }
            if rc < 0 {
                return Err(io::Error::last_os_error());
            }
            if (out.flags & libc::EV_ERROR) != 0 && out.data != 0 {
                return Err(io::Error::from_raw_os_error(out.data as i32));
            }
            Ok(())
        }

        #[cfg(not(unix))]
        pub fn poll(_timeout: Duration) -> io::Result<bool> {
            Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "terminal events require unix",
            ))
        }

        #[cfg(unix)]
        fn poll_fds(stdin_fd: libc::c_int, timeout: Option<Duration>) -> io::Result<(bool, bool)> {
            let winch = if stdin_fd == libc::STDIN_FILENO {
                winch_read_fd()
            } else {
                None
            };
            let deadline = timeout.map(|d| Instant::now() + d);

            loop {
                let timeout_ms: libc::c_int = match deadline {
                    None => -1,
                    Some(dl) => {
                        let rem = dl.saturating_duration_since(Instant::now());
                        rem.as_millis().min(i32::MAX as u128) as libc::c_int
                    }
                };
                let mut pfds = [
                    libc::pollfd {
                        fd: stdin_fd,
                        events: libc::POLLIN,
                        revents: 0,
                    },
                    libc::pollfd {
                        fd: winch.unwrap_or(-1),
                        events: libc::POLLIN,
                        revents: 0,
                    },
                ];
                let nfds: libc::nfds_t = if winch.is_some() { 2 } else { 1 };
                let rc = unsafe { libc::poll(pfds.as_mut_ptr(), nfds, timeout_ms) };
                if rc < 0 {
                    let err = io::Error::last_os_error();
                    if err.kind() == io::ErrorKind::Interrupted {
                        continue;
                    }
                    return Err(err);
                }
                if (pfds[0].revents & libc::POLLNVAL) != 0 {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "terminal descriptor invalid",
                    ));
                }
                let stdin_ready =
                    (pfds[0].revents & (libc::POLLIN | libc::POLLHUP | libc::POLLERR)) != 0;
                let winch_ready = winch.is_some() && (pfds[1].revents & libc::POLLIN) != 0;
                return Ok((stdin_ready, winch_ready));
            }
        }

        #[cfg(unix)]
        pub fn read() -> io::Result<Event> {
            read_from(libc::STDIN_FILENO, state())
        }

        #[cfg(unix)]
        pub(crate) fn read_from(
            stdin_fd: libc::c_int,
            reader: &Mutex<ReaderState>,
        ) -> io::Result<Event> {
            loop {
                {
                    let mut guard = reader.lock().unwrap_or_else(|e| e.into_inner());
                    if let Some(ev) = guard.queued.pop_front() {
                        return Ok(ev);
                    }
                    if !guard.buf.is_empty() {
                        // Wait briefly if we have an incomplete escape prefix.
                        drop(guard);
                        let (more, _) = poll_fds(stdin_fd, Some(Duration::from_millis(25)))?;
                        let mut guard = reader.lock().unwrap_or_else(|e| e.into_inner());
                        let eof = if more {
                            read_available_into(stdin_fd, &mut guard.buf)? == 0
                        } else {
                            false
                        };
                        let st = &mut *guard;
                        decode_events(&mut st.buf, &mut st.queued, !more || eof);
                        if let Some(ev) = st.queued.pop_front() {
                            return Ok(ev);
                        }
                        if eof {
                            return Err(io::Error::new(
                                io::ErrorKind::UnexpectedEof,
                                "terminal closed",
                            ));
                        }
                    }
                }

                let (stdin_ready, winch_ready) = poll_fds(stdin_fd, None)?;
                if winch_ready && let Some(wfd) = winch_read_fd() {
                    let mut sink = [0u8; 64];
                    while unsafe { libc::read(wfd, sink.as_mut_ptr().cast(), sink.len()) } > 0 {}
                    let (cols, rows) = super::terminal::size()?;
                    return Ok(Event::Resize(cols, rows));
                }
                if stdin_ready {
                    let mut guard = reader.lock().unwrap_or_else(|e| e.into_inner());
                    let eof = read_available_into(stdin_fd, &mut guard.buf)? == 0;
                    let st = &mut *guard;
                    decode_events(&mut st.buf, &mut st.queued, eof);
                    if let Some(ev) = st.queued.pop_front() {
                        return Ok(ev);
                    }
                    if eof {
                        return Err(io::Error::new(
                            io::ErrorKind::UnexpectedEof,
                            "terminal closed",
                        ));
                    }
                }
            }
        }

        #[cfg(not(unix))]
        pub fn read() -> io::Result<Event> {
            Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "terminal events require unix",
            ))
        }

        #[cfg(unix)]
        fn read_available_into(fd: libc::c_int, buf: &mut Vec<u8>) -> io::Result<usize> {
            let mut tmp = [0u8; 256];
            loop {
                let rc = unsafe { libc::read(fd, tmp.as_mut_ptr().cast(), tmp.len()) };
                if rc < 0 {
                    let err = io::Error::last_os_error();
                    if err.kind() == io::ErrorKind::Interrupted {
                        continue;
                    }
                    return Err(err);
                }
                let n = rc as usize;
                buf.extend_from_slice(&tmp[..n]);
                return Ok(n);
            }
        }

        pub(crate) fn decode_events(
            buf: &mut Vec<u8>,
            out: &mut std::collections::VecDeque<Event>,
            flush_incomplete: bool,
        ) {
            let mut pos = 0;
            while pos < buf.len() {
                match decode_one(&buf[pos..], flush_incomplete) {
                    Decode::Event(ev, used) => {
                        out.push_back(ev);
                        pos += used;
                    }
                    Decode::Skip(used) => {
                        pos += used;
                    }
                    Decode::NeedMore => break,
                }
            }
            if pos > 0 {
                buf.drain(..pos);
            }
        }

        enum Decode {
            Event(Event, usize),
            Skip(usize),
            NeedMore,
        }

        fn decode_one(bytes: &[u8], flush_incomplete: bool) -> Decode {
            let b0 = bytes[0];
            if b0 == 0x1b {
                return decode_esc(bytes, flush_incomplete);
            }
            match b0 {
                b'\r' | b'\n' => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Enter, KeyModifiers::NONE)),
                    1,
                ),
                b'\t' => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Tab, KeyModifiers::NONE)),
                    1,
                ),
                0x7f | 0x08 => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Backspace, KeyModifiers::NONE)),
                    1,
                ),
                0x00 => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Char(' '), KeyModifiers::CONTROL)),
                    1,
                ),
                0x01..=0x1a => {
                    let ch = (b0 - 1 + b'a') as char;
                    Decode::Event(
                        Event::Key(KeyEvent::new(KeyCode::Char(ch), KeyModifiers::CONTROL)),
                        1,
                    )
                }
                0x1c..=0x1f => Decode::Skip(1),
                0x20..=0x7e => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Char(b0 as char), KeyModifiers::NONE)),
                    1,
                ),
                _ => decode_utf8(bytes, KeyModifiers::NONE, flush_incomplete),
            }
        }

        fn decode_utf8(bytes: &[u8], mods: KeyModifiers, flush_incomplete: bool) -> Decode {
            let expected = match bytes[0] {
                0x00..=0x7f => 1,
                0xc0..=0xdf => 2,
                0xe0..=0xef => 3,
                0xf0..=0xf7 => 4,
                _ => return Decode::Skip(1),
            };
            if bytes.len() < expected {
                return if flush_incomplete {
                    Decode::Skip(bytes.len())
                } else {
                    Decode::NeedMore
                };
            }
            match std::str::from_utf8(&bytes[..expected]) {
                Ok(s) => {
                    let ch = s.chars().next().unwrap_or('\u{fffd}');
                    Decode::Event(Event::Key(KeyEvent::new(KeyCode::Char(ch), mods)), expected)
                }
                Err(_) => Decode::Skip(1),
            }
        }

        fn decode_esc(bytes: &[u8], flush_incomplete: bool) -> Decode {
            if bytes.len() == 1 {
                return if flush_incomplete {
                    Decode::Event(
                        Event::Key(KeyEvent::new(KeyCode::Esc, KeyModifiers::NONE)),
                        1,
                    )
                } else {
                    Decode::NeedMore
                };
            }
            match bytes[1] {
                b'[' => decode_csi(bytes, flush_incomplete),
                b'O' => decode_ss3(bytes, flush_incomplete),
                0x1b => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Esc, KeyModifiers::NONE)),
                    1,
                ),
                b @ 0x20..=0x7e => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Char(b as char), KeyModifiers::ALT)),
                    2,
                ),
                0x7f | 0x08 => Decode::Event(
                    Event::Key(KeyEvent::new(KeyCode::Backspace, KeyModifiers::ALT)),
                    2,
                ),
                b @ 0x01..=0x1a => {
                    let ch = (b - 1 + b'a') as char;
                    Decode::Event(
                        Event::Key(KeyEvent::new(
                            KeyCode::Char(ch),
                            KeyModifiers::CONTROL | KeyModifiers::ALT,
                        )),
                        2,
                    )
                }
                _ => match decode_utf8(&bytes[1..], KeyModifiers::ALT, flush_incomplete) {
                    Decode::Event(ev, n) => Decode::Event(ev, 1 + n),
                    Decode::Skip(n) => Decode::Skip(1 + n),
                    Decode::NeedMore => Decode::NeedMore,
                },
            }
        }

        fn decode_ss3(bytes: &[u8], flush_incomplete: bool) -> Decode {
            if bytes.len() < 3 {
                return if flush_incomplete {
                    Decode::Skip(bytes.len())
                } else {
                    Decode::NeedMore
                };
            }
            let code = match bytes[2] {
                b'A' => KeyCode::Up,
                b'B' => KeyCode::Down,
                b'C' => KeyCode::Right,
                b'D' => KeyCode::Left,
                b'H' => KeyCode::Home,
                b'F' => KeyCode::End,
                b'P' => KeyCode::F(1),
                b'Q' => KeyCode::F(2),
                b'R' => KeyCode::F(3),
                b'S' => KeyCode::F(4),
                _ => return Decode::Skip(3),
            };
            Decode::Event(Event::Key(KeyEvent::new(code, KeyModifiers::NONE)), 3)
        }

        fn decode_csi(bytes: &[u8], flush_incomplete: bool) -> Decode {
            // Legacy X10 mouse: ESC [ M Cb Cx Cy
            if bytes.get(2) == Some(&b'M') {
                if bytes.len() < 6 {
                    return if flush_incomplete {
                        Decode::Skip(bytes.len())
                    } else {
                        Decode::NeedMore
                    };
                }
                let cb = bytes[3].saturating_sub(32);
                let col = u16::from(bytes[4].saturating_sub(33));
                let row = u16::from(bytes[5].saturating_sub(33));
                if let Some(ev) = parse_mouse_button(cb, col, row, cb & 3 != 3) {
                    return Decode::Event(Event::Mouse(ev), 6);
                }
                return Decode::Skip(6);
            }

            // Standard CSI: ESC [ <params> <final byte 0x40..=0x7e>
            let mut idx = 2;
            while idx < bytes.len() && !(0x40..=0x7e).contains(&bytes[idx]) {
                idx += 1;
            }
            if idx >= bytes.len() {
                return if flush_incomplete {
                    Decode::Skip(bytes.len())
                } else {
                    Decode::NeedMore
                };
            }

            let final_byte = bytes[idx];
            let params = &bytes[2..idx];
            let total = idx + 1;

            // SGR 1006 mouse: ESC [ < Cb ; Cx ; Cy M / m
            if params.first() == Some(&b'<') && matches!(final_byte, b'M' | b'm') {
                if let Ok(s) = std::str::from_utf8(&params[1..]) {
                    let mut nums = s.split(';').filter_map(|p| p.parse::<u16>().ok());
                    if let (Some(cb), Some(cx), Some(cy)) = (nums.next(), nums.next(), nums.next())
                        && let Some(ev) = parse_mouse_button(
                            cb as u8,
                            cx.saturating_sub(1),
                            cy.saturating_sub(1),
                            final_byte == b'M',
                        )
                    {
                        return Decode::Event(Event::Mouse(ev), total);
                    }
                }
                return Decode::Skip(total);
            }

            let Ok(param_str) = std::str::from_utf8(params) else {
                return Decode::Skip(total);
            };
            let nums: Vec<u32> = if param_str.is_empty() {
                Vec::new()
            } else {
                param_str
                    .split(';')
                    .map(|s| {
                        s.split(':')
                            .next()
                            .unwrap_or("")
                            .parse::<u32>()
                            .unwrap_or(0)
                    })
                    .collect()
            };
            let mods = nums
                .get(1)
                .copied()
                .map(xterm_modifiers)
                .unwrap_or(KeyModifiers::NONE);

            let key = match final_byte {
                b'A' => Some((KeyCode::Up, mods)),
                b'B' => Some((KeyCode::Down, mods)),
                b'C' => Some((KeyCode::Right, mods)),
                b'D' => Some((KeyCode::Left, mods)),
                b'H' => Some((KeyCode::Home, mods)),
                b'F' => Some((KeyCode::End, mods)),
                b'Z' => Some((KeyCode::BackTab, mods | KeyModifiers::SHIFT)),
                b'P' => Some((KeyCode::F(1), mods)),
                b'Q' => Some((KeyCode::F(2), mods)),
                b'R' => Some((KeyCode::F(3), mods)),
                b'S' => Some((KeyCode::F(4), mods)),
                b'u' => nums.first().copied().and_then(|cp| {
                    let code = match cp {
                        9 => KeyCode::Tab,
                        13 => KeyCode::Enter,
                        27 => KeyCode::Esc,
                        127 => KeyCode::Backspace,
                        _ => KeyCode::Char(char::from_u32(cp)?),
                    };
                    Some((code, mods))
                }),
                b'~' => {
                    let n = nums.first().copied().unwrap_or(0);
                    let code = match n {
                        1 | 7 => Some(KeyCode::Home),
                        2 => Some(KeyCode::Insert),
                        3 => Some(KeyCode::Delete),
                        4 | 8 => Some(KeyCode::End),
                        5 => Some(KeyCode::PageUp),
                        6 => Some(KeyCode::PageDown),
                        11..=15 => Some(KeyCode::F((n - 10) as u8)),
                        17..=21 => Some(KeyCode::F((n - 11) as u8)),
                        23..=24 => Some(KeyCode::F((n - 12) as u8)),
                        _ => None,
                    };
                    code.map(|c| (c, mods))
                }
                _ => None,
            };

            match key {
                Some((code, m)) => Decode::Event(Event::Key(KeyEvent::new(code, m)), total),
                None => Decode::Skip(total),
            }
        }

        fn xterm_modifiers(param: u32) -> KeyModifiers {
            let bits = param.saturating_sub(1);
            let mut m = KeyModifiers::NONE;
            if (bits & 1) != 0 {
                m |= KeyModifiers::SHIFT;
            }
            if (bits & 2) != 0 {
                m |= KeyModifiers::ALT;
            }
            if (bits & 4) != 0 {
                m |= KeyModifiers::CONTROL;
            }
            m
        }

        fn parse_mouse_button(cb: u8, column: u16, row: u16, pressed: bool) -> Option<MouseEvent> {
            let mut modifiers = KeyModifiers::NONE;
            if (cb & 4) != 0 {
                modifiers |= KeyModifiers::SHIFT;
            }
            if (cb & 8) != 0 {
                modifiers |= KeyModifiers::ALT;
            }
            if (cb & 16) != 0 {
                modifiers |= KeyModifiers::CONTROL;
            }

            let kind = if (cb & 64) != 0 {
                match cb & 3 {
                    0 => MouseEventKind::ScrollUp,
                    1 => MouseEventKind::ScrollDown,
                    2 => MouseEventKind::ScrollLeft,
                    3 => MouseEventKind::ScrollRight,
                    _ => return None,
                }
            } else {
                let button = match cb & 3 {
                    0 => Some(MouseButton::Left),
                    1 => Some(MouseButton::Middle),
                    2 => Some(MouseButton::Right),
                    _ => None,
                };
                let motion = (cb & 32) != 0;
                match (button, motion, pressed) {
                    (Some(btn), true, _) => MouseEventKind::Drag(btn),
                    (None, true, _) => MouseEventKind::Moved,
                    (Some(btn), false, true) => MouseEventKind::Down(btn),
                    (Some(btn), false, false) => MouseEventKind::Up(btn),
                    (None, false, _) => MouseEventKind::Up(MouseButton::Left),
                }
            };

            Some(MouseEvent {
                kind,
                column,
                row,
                modifiers,
            })
        }
    }
}

pub mod render {
    use std::borrow::Cow;
    use std::io::{self, Write};
    use std::ops::{BitOr, BitOrAssign};

    pub mod layout {
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
        pub struct Rect {
            pub x: u16,
            pub y: u16,
            pub width: u16,
            pub height: u16,
        }

        impl Rect {
            pub const fn new(x: u16, y: u16, width: u16, height: u16) -> Self {
                Self {
                    x,
                    y,
                    width,
                    height,
                }
            }

            pub const fn left(self) -> u16 {
                self.x
            }

            pub const fn right(self) -> u16 {
                self.x.saturating_add(self.width)
            }

            pub const fn top(self) -> u16 {
                self.y
            }

            pub const fn bottom(self) -> u16 {
                self.y.saturating_add(self.height)
            }
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq)]
        pub enum Constraint {
            Length(u16),
            Min(u16),
            Percentage(u16),
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq)]
        enum Direction {
            Horizontal,
            Vertical,
        }

        pub struct Layout {
            direction: Direction,
            constraints: Vec<Constraint>,
        }

        impl Layout {
            pub fn vertical<I: IntoIterator<Item = Constraint>>(constraints: I) -> Self {
                Self {
                    direction: Direction::Vertical,
                    constraints: constraints.into_iter().collect(),
                }
            }

            pub fn horizontal<I: IntoIterator<Item = Constraint>>(constraints: I) -> Self {
                Self {
                    direction: Direction::Horizontal,
                    constraints: constraints.into_iter().collect(),
                }
            }

            pub fn areas<const N: usize>(&self, area: Rect) -> [Rect; N] {
                assert_eq!(
                    self.constraints.len(),
                    N,
                    "Layout::areas constraint count mismatch"
                );
                let total = match self.direction {
                    Direction::Horizontal => area.width,
                    Direction::Vertical => area.height,
                };
                let sizes = solve_constraints(&self.constraints, total);
                let mut out = [Rect::default(); N];
                let mut cursor = match self.direction {
                    Direction::Horizontal => area.x,
                    Direction::Vertical => area.y,
                };
                for i in 0..N {
                    let sz = sizes[i];
                    out[i] = match self.direction {
                        Direction::Horizontal => Rect::new(cursor, area.y, sz, area.height),
                        Direction::Vertical => Rect::new(area.x, cursor, area.width, sz),
                    };
                    cursor = cursor.saturating_add(sz);
                }
                out
            }
        }

        fn solve_constraints(constraints: &[Constraint], total: u16) -> Vec<u16> {
            let n = constraints.len();
            let mut sizes = vec![0u16; n];
            if n == 0 || total == 0 {
                return sizes;
            }

            // First pass: satisfy fixed `Length`, `Percentage`, and `Min` floors,
            // bounded by remaining space so small viewports never overflow.
            let mut remaining = total;
            let mut flex_indices = Vec::new();
            for (i, c) in constraints.iter().enumerate() {
                let want = match *c {
                    Constraint::Length(v) => v,
                    Constraint::Percentage(p) => {
                        ((u32::from(total) * u32::from(p.min(100))) / 100) as u16
                    }
                    Constraint::Min(m) => {
                        flex_indices.push(i);
                        m
                    }
                };
                let alloc = want.min(remaining);
                sizes[i] = alloc;
                remaining = remaining.saturating_sub(alloc);
            }

            // Second pass: distribute leftover space across `Min` items (or
            // trailing Percentage items if there are no `Min` items).
            if remaining > 0 {
                if !flex_indices.is_empty() {
                    let count = flex_indices.len() as u16;
                    let base = remaining / count;
                    let extra = remaining % count;
                    for (k, &idx) in flex_indices.iter().enumerate() {
                        sizes[idx] += base + u16::from((k as u16) < extra);
                    }
                } else if let Some(last_pct) = constraints
                    .iter()
                    .rposition(|c| matches!(c, Constraint::Percentage(_)))
                {
                    sizes[last_pct] += remaining;
                }
            }

            sizes
        }
    }

    pub mod style {
        use super::*;

        #[derive(Debug, Clone, Copy, PartialEq, Eq)]
        pub enum Color {
            Reset,
            Black,
            Red,
            Green,
            Yellow,
            Blue,
            Magenta,
            Cyan,
            Gray,
            DarkGray,
            LightRed,
            LightGreen,
            LightYellow,
            LightBlue,
            LightMagenta,
            LightCyan,
            White,
            Indexed(u8),
            Rgb(u8, u8, u8),
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
        pub struct Modifier(u8);

        impl Modifier {
            pub const NONE: Self = Self(0);
            pub const BOLD: Self = Self(1 << 0);
            pub const DIM: Self = Self(1 << 1);
            pub const ITALIC: Self = Self(1 << 2);
            pub const UNDERLINED: Self = Self(1 << 3);
            pub const REVERSED: Self = Self(1 << 4);

            pub const fn contains(self, other: Self) -> bool {
                (self.0 & other.0) == other.0
            }

            pub const fn is_empty(self) -> bool {
                self.0 == 0
            }
        }

        impl BitOr for Modifier {
            type Output = Self;
            fn bitor(self, rhs: Self) -> Self {
                Self(self.0 | rhs.0)
            }
        }

        impl BitOrAssign for Modifier {
            fn bitor_assign(&mut self, rhs: Self) {
                self.0 |= rhs.0;
            }
        }

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
        pub struct Style {
            pub fg: Option<Color>,
            pub bg: Option<Color>,
            pub add_modifier: Modifier,
        }

        impl Style {
            pub const fn new() -> Self {
                Self {
                    fg: None,
                    bg: None,
                    add_modifier: Modifier::NONE,
                }
            }

            pub const fn fg(mut self, color: Color) -> Self {
                self.fg = Some(color);
                self
            }

            pub const fn bg(mut self, color: Color) -> Self {
                self.bg = Some(color);
                self
            }

            pub const fn add_modifier(mut self, modifier: Modifier) -> Self {
                self.add_modifier = Modifier(self.add_modifier.0 | modifier.0);
                self
            }

            pub const fn patch(self, other: Self) -> Self {
                Self {
                    fg: match other.fg {
                        Some(c) => Some(c),
                        None => self.fg,
                    },
                    bg: match other.bg {
                        Some(c) => Some(c),
                        None => self.bg,
                    },
                    add_modifier: Modifier(self.add_modifier.0 | other.add_modifier.0),
                }
            }
        }
    }

    pub mod text {
        use super::Cow;
        use super::style::Style;

        #[derive(Debug, Clone, PartialEq, Eq)]
        pub struct Span<'a> {
            pub content: Cow<'a, str>,
            pub style: Style,
        }

        impl<'a> Span<'a> {
            pub fn raw<T: Into<Cow<'a, str>>>(content: T) -> Self {
                Self {
                    content: content.into(),
                    style: Style::new(),
                }
            }

            pub fn styled<T: Into<Cow<'a, str>>>(content: T, style: Style) -> Self {
                Self {
                    content: content.into(),
                    style,
                }
            }
        }

        impl<'a> From<&'a str> for Span<'a> {
            fn from(s: &'a str) -> Self {
                Self::raw(s)
            }
        }

        impl From<String> for Span<'static> {
            fn from(s: String) -> Self {
                Self::raw(s)
            }
        }

        #[derive(Debug, Clone, PartialEq, Eq, Default)]
        pub struct Line<'a> {
            pub spans: Vec<Span<'a>>,
            pub style: Style,
        }

        impl<'a> Line<'a> {
            pub fn style(mut self, style: Style) -> Self {
                self.style = self.style.patch(style);
                self
            }
        }

        impl<'a> From<Span<'a>> for Line<'a> {
            fn from(span: Span<'a>) -> Self {
                Self {
                    spans: vec![span],
                    style: Style::new(),
                }
            }
        }

        impl<'a> From<Vec<Span<'a>>> for Line<'a> {
            fn from(spans: Vec<Span<'a>>) -> Self {
                Self {
                    spans,
                    style: Style::new(),
                }
            }
        }

        impl<'a> From<&'a str> for Line<'a> {
            fn from(s: &'a str) -> Self {
                Self::from(Span::raw(s))
            }
        }

        impl From<String> for Line<'static> {
            fn from(s: String) -> Self {
                Self::from(Span::raw(s))
            }
        }

        #[derive(Debug, Clone, PartialEq, Eq, Default)]
        pub struct Text<'a> {
            pub lines: Vec<Line<'a>>,
        }

        impl<'a> From<Line<'a>> for Text<'a> {
            fn from(line: Line<'a>) -> Self {
                Self { lines: vec![line] }
            }
        }

        impl<'a> From<Span<'a>> for Text<'a> {
            fn from(span: Span<'a>) -> Self {
                Self {
                    lines: vec![Line::from(span)],
                }
            }
        }

        impl<'a> From<Vec<Line<'a>>> for Text<'a> {
            fn from(lines: Vec<Line<'a>>) -> Self {
                Self { lines }
            }
        }

        impl<'a> From<&'a str> for Text<'a> {
            fn from(s: &'a str) -> Self {
                Self {
                    lines: s.lines().map(Line::from).collect(),
                }
            }
        }
    }

    pub trait Widget {
        fn render(self, area: layout::Rect, buf: &mut Buffer);
    }

    pub mod widgets {
        use super::layout::Rect;
        use super::style::Style;
        use super::text::{Line, Text};
        use super::{Buffer, Widget, char_width};
        use std::ops::{BitOr, BitOrAssign};

        #[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
        pub struct Borders(u8);

        impl Borders {
            pub const NONE: Self = Self(0);
            pub const TOP: Self = Self(1 << 0);
            pub const RIGHT: Self = Self(1 << 1);
            pub const BOTTOM: Self = Self(1 << 2);
            pub const LEFT: Self = Self(1 << 3);
            pub const ALL: Self = Self(0b1111);

            pub const fn contains(self, other: Self) -> bool {
                (self.0 & other.0) == other.0
            }
        }

        impl BitOr for Borders {
            type Output = Self;
            fn bitor(self, rhs: Self) -> Self {
                Self(self.0 | rhs.0)
            }
        }

        impl BitOrAssign for Borders {
            fn bitor_assign(&mut self, rhs: Self) {
                self.0 |= rhs.0;
            }
        }

        #[derive(Debug, Clone, Default)]
        pub struct Block<'a> {
            borders: Borders,
            border_style: Style,
            style: Style,
            title: Option<Line<'a>>,
        }

        impl<'a> Block<'a> {
            pub fn new() -> Self {
                Self::default()
            }

            pub fn bordered() -> Self {
                Self {
                    borders: Borders::ALL,
                    ..Self::default()
                }
            }

            pub fn borders(mut self, borders: Borders) -> Self {
                self.borders = borders;
                self
            }

            pub fn border_style(mut self, style: Style) -> Self {
                self.border_style = style;
                self
            }

            pub fn style(mut self, style: Style) -> Self {
                self.style = style;
                self
            }

            pub fn title<T: Into<Line<'a>>>(mut self, title: T) -> Self {
                self.title = Some(title.into());
                self
            }

            pub fn inner(&self, area: Rect) -> Rect {
                let left = u16::from(self.borders.contains(Borders::LEFT));
                let right = u16::from(self.borders.contains(Borders::RIGHT));
                let top = u16::from(self.borders.contains(Borders::TOP));
                let bottom = u16::from(self.borders.contains(Borders::BOTTOM));
                Rect {
                    x: area.x.saturating_add(left).min(area.right()),
                    y: area.y.saturating_add(top).min(area.bottom()),
                    width: area.width.saturating_sub(left + right),
                    height: area.height.saturating_sub(top + bottom),
                }
            }
        }

        impl Widget for Block<'_> {
            fn render(self, area: Rect, buf: &mut Buffer) {
                if area.width == 0 || area.height == 0 {
                    return;
                }
                buf.set_style(area, self.style);

                let top = self.borders.contains(Borders::TOP);
                let bottom = self.borders.contains(Borders::BOTTOM);
                let left = self.borders.contains(Borders::LEFT);
                let right = self.borders.contains(Borders::RIGHT);

                if top {
                    for x in area.left()..area.right() {
                        buf.put_char(x, area.top(), '─', self.border_style);
                    }
                }
                if bottom {
                    let y = area.bottom() - 1;
                    for x in area.left()..area.right() {
                        buf.put_char(x, y, '─', self.border_style);
                    }
                }
                if left {
                    for y in area.top()..area.bottom() {
                        buf.put_char(area.left(), y, '│', self.border_style);
                    }
                }
                if right {
                    let x = area.right() - 1;
                    for y in area.top()..area.bottom() {
                        buf.put_char(x, y, '│', self.border_style);
                    }
                }
                if top && left {
                    buf.put_char(area.left(), area.top(), '┌', self.border_style);
                }
                if top && right {
                    buf.put_char(area.right() - 1, area.top(), '┐', self.border_style);
                }
                if bottom && left {
                    buf.put_char(area.left(), area.bottom() - 1, '└', self.border_style);
                }
                if bottom && right {
                    buf.put_char(area.right() - 1, area.bottom() - 1, '┘', self.border_style);
                }

                if let Some(title) = self.title {
                    let dx = u16::from(left);
                    let max_w = area.width.saturating_sub(dx + u16::from(right));
                    if max_w > 0 {
                        let base = self.border_style.patch(title.style);
                        let mut x = area.left() + dx;
                        let limit = x + max_w;
                        for span in &title.spans {
                            let st = base.patch(span.style);
                            for ch in span.content.chars() {
                                let w = char_width(ch) as u16;
                                if w == 0 {
                                    continue;
                                }
                                if x + w > limit {
                                    break;
                                }
                                buf.put_char(x, area.top(), ch, st);
                                x += w;
                            }
                        }
                    }
                }
            }
        }

        pub struct Clear;

        impl Widget for Clear {
            fn render(self, area: Rect, buf: &mut Buffer) {
                buf.reset_area(area);
            }
        }

        #[derive(Debug, Clone, Copy)]
        pub struct Wrap {
            pub trim: bool,
        }

        pub struct Paragraph<'a> {
            text: Text<'a>,
            block: Option<Block<'a>>,
            style: Style,
            wrap: Option<Wrap>,
        }

        impl<'a> Paragraph<'a> {
            pub fn new<T: Into<Text<'a>>>(text: T) -> Self {
                Self {
                    text: text.into(),
                    block: None,
                    style: Style::new(),
                    wrap: None,
                }
            }

            pub fn block(mut self, block: Block<'a>) -> Self {
                self.block = Some(block);
                self
            }

            pub fn style(mut self, style: Style) -> Self {
                self.style = style;
                self
            }

            pub fn wrap(mut self, wrap: Wrap) -> Self {
                self.wrap = Some(wrap);
                self
            }
        }

        impl Widget for Paragraph<'_> {
            fn render(self, area: Rect, buf: &mut Buffer) {
                if area.width == 0 || area.height == 0 {
                    return;
                }
                buf.set_style(area, self.style);
                let inner = match self.block {
                    Some(b) => {
                        let inner = b.inner(area);
                        b.render(area, buf);
                        inner
                    }
                    None => area,
                };
                if inner.width == 0 || inner.height == 0 {
                    return;
                }

                let mut y = inner.top();
                let bottom = inner.bottom();
                let right = inner.right();
                let wrap = self.wrap;

                for line in &self.text.lines {
                    if y >= bottom {
                        break;
                    }
                    buf.set_style(Rect::new(inner.left(), y, inner.width, 1), line.style);
                    let mut x = inner.left();
                    let mut clipped = false;

                    for span in &line.spans {
                        if clipped {
                            break;
                        }
                        let st = span.style;
                        for ch in span.content.chars() {
                            if ch == '\n' || ch == '\r' {
                                continue;
                            }
                            let w = char_width(ch) as u16;
                            if w == 0 {
                                continue;
                            }
                            if x + w > right {
                                if let Some(wcfg) = wrap {
                                    y += 1;
                                    if y >= bottom {
                                        clipped = true;
                                        break;
                                    }
                                    buf.set_style(
                                        Rect::new(inner.left(), y, inner.width, 1),
                                        line.style,
                                    );
                                    x = inner.left();
                                    if wcfg.trim && ch == ' ' {
                                        continue;
                                    }
                                    if x + w > right {
                                        clipped = true;
                                        break;
                                    }
                                } else {
                                    clipped = true;
                                    break;
                                }
                            }
                            buf.put_char(x, y, ch, st);
                            x += w;
                        }
                    }
                    y += 1;
                }
            }
        }
    }

    pub mod backend {
        use std::io::Write;

        pub struct AnsiBackend<W: Write> {
            pub(crate) writer: W,
        }

        impl<W: Write> AnsiBackend<W> {
            pub fn new(writer: W) -> Self {
                Self { writer }
            }
        }
    }

    #[derive(Clone, PartialEq, Eq)]
    struct Cell {
        ch: char,
        style: style::Style,
        continuation: bool,
    }

    impl Default for Cell {
        fn default() -> Self {
            Self {
                ch: ' ',
                style: style::Style::new(),
                continuation: false,
            }
        }
    }

    pub struct Buffer {
        area: layout::Rect,
        cells: Vec<Cell>,
    }

    impl Buffer {
        fn empty(area: layout::Rect) -> Self {
            let count = usize::from(area.width) * usize::from(area.height);
            Self {
                area,
                cells: vec![Cell::default(); count],
            }
        }

        fn clear(&mut self) {
            for cell in &mut self.cells {
                *cell = Cell::default();
            }
        }

        fn index_of(&self, x: u16, y: u16) -> Option<usize> {
            if x < self.area.left()
                || x >= self.area.right()
                || y < self.area.top()
                || y >= self.area.bottom()
            {
                return None;
            }
            let rx = usize::from(x - self.area.x);
            let ry = usize::from(y - self.area.y);
            Some(ry * usize::from(self.area.width) + rx)
        }

        pub(crate) fn reset_area(&mut self, area: layout::Rect) {
            let x0 = area.left().max(self.area.left());
            let x1 = area.right().min(self.area.right());
            let y0 = area.top().max(self.area.top());
            let y1 = area.bottom().min(self.area.bottom());
            for y in y0..y1 {
                for x in x0..x1 {
                    if let Some(i) = self.index_of(x, y) {
                        self.cells[i] = Cell::default();
                    }
                }
            }
        }

        pub(crate) fn set_style(&mut self, area: layout::Rect, s: style::Style) {
            if s == style::Style::new() {
                return;
            }
            let x0 = area.left().max(self.area.left());
            let x1 = area.right().min(self.area.right());
            let y0 = area.top().max(self.area.top());
            let y1 = area.bottom().min(self.area.bottom());
            for y in y0..y1 {
                for x in x0..x1 {
                    if let Some(i) = self.index_of(x, y) {
                        self.cells[i].style = self.cells[i].style.patch(s);
                    }
                }
            }
        }

        pub(crate) fn put_char(&mut self, x: u16, y: u16, ch: char, s: style::Style) {
            let Some(i) = self.index_of(x, y) else {
                return;
            };
            // Overwriting the right half of a wide character blanks its left half.
            if self.cells[i].continuation && x > self.area.left() {
                self.cells[i - 1].ch = ' ';
                self.cells[i - 1].continuation = false;
            }
            // Overwriting a wide character's left half clears its right continuation.
            if x + 1 < self.area.right() && self.cells[i + 1].continuation {
                self.cells[i + 1].ch = ' ';
                self.cells[i + 1].continuation = false;
            }

            let merged = self.cells[i].style.patch(s);
            let w = char_width(ch);
            self.cells[i] = Cell {
                ch,
                style: merged,
                continuation: false,
            };
            if w == 2 && x + 1 < self.area.right() {
                let right_style = self.cells[i + 1].style.patch(s);
                self.cells[i + 1] = Cell {
                    ch: ' ',
                    style: right_style,
                    continuation: true,
                };
            }
        }
    }

    pub struct Frame<'a> {
        area: layout::Rect,
        buf: &'a mut Buffer,
    }

    impl Frame<'_> {
        pub fn area(&self) -> layout::Rect {
            self.area
        }

        pub fn render_widget<W: Widget>(&mut self, widget: W, area: layout::Rect) {
            widget.render(area, self.buf);
        }
    }

    pub struct Terminal<B> {
        backend: B,
        buffers: [Buffer; 2],
        current: usize,
        size: layout::Rect,
        first_draw: bool,
    }

    impl<W: Write> Terminal<backend::AnsiBackend<W>> {
        pub fn new(backend: backend::AnsiBackend<W>) -> io::Result<Self> {
            let (w, h) = super::term::terminal::size()?;
            let size = layout::Rect::new(0, 0, w, h);
            Ok(Self {
                backend,
                buffers: [Buffer::empty(size), Buffer::empty(size)],
                current: 0,
                size,
                first_draw: true,
            })
        }

        pub fn draw<F>(&mut self, f: F) -> io::Result<()>
        where
            F: FnOnce(&mut Frame<'_>),
        {
            let (w, h) = super::term::terminal::size()?;
            let size = layout::Rect::new(0, 0, w, h);
            let resized = size != self.size;
            if resized {
                self.size = size;
                self.buffers = [Buffer::empty(size), Buffer::empty(size)];
                self.current = 0;
                self.first_draw = true;
            }

            self.buffers[self.current].clear();
            {
                let mut frame = Frame {
                    area: self.size,
                    buf: &mut self.buffers[self.current],
                };
                f(&mut frame);
            }

            let prev = 1 - self.current;
            let mut out = Vec::with_capacity(4096);
            // Synchronized output (DEC mode 2026) avoids tearing on supporting
            // terminals and is ignored by legacy ones.
            out.extend_from_slice(b"\x1b[?2026h");
            if resized {
                out.extend_from_slice(b"\x1b[0m\x1b[2J");
            }

            let full = self.first_draw;
            emit_diff(
                &self.buffers[prev],
                &self.buffers[self.current],
                full,
                &mut out,
            );
            out.extend_from_slice(b"\x1b[0m\x1b[?2026l");

            self.backend.writer.write_all(&out)?;
            self.backend.writer.flush()?;

            self.first_draw = false;
            self.current = prev;
            Ok(())
        }
    }

    fn emit_diff(prev: &Buffer, curr: &Buffer, full: bool, out: &mut Vec<u8>) {
        let width = curr.area.width;
        let height = curr.area.height;
        let mut cur_pos: Option<(u16, u16)> = None;
        let mut cur_style: Option<style::Style> = None;

        for y in 0..height {
            let row_base = usize::from(y) * usize::from(width);
            for x in 0..width {
                let idx = row_base + usize::from(x);
                let cell = &curr.cells[idx];
                if cell.continuation {
                    continue;
                }
                if !full && cell == &prev.cells[idx] {
                    continue;
                }

                if cur_pos != Some((x, y)) {
                    write_cup(out, x, y);
                }
                if cur_style != Some(cell.style) {
                    write_style(out, cell.style);
                    cur_style = Some(cell.style);
                }

                let mut utf8 = [0u8; 4];
                out.extend_from_slice(cell.ch.encode_utf8(&mut utf8).as_bytes());
                let adv = char_width(cell.ch) as u16;
                cur_pos = Some((x.saturating_add(adv), y));
            }
        }
    }

    fn write_cup(out: &mut Vec<u8>, x: u16, y: u16) {
        out.extend_from_slice(b"\x1b[");
        push_u16(out, y.saturating_add(1));
        out.push(b';');
        push_u16(out, x.saturating_add(1));
        out.push(b'H');
    }

    fn write_style(out: &mut Vec<u8>, s: style::Style) {
        use style::Modifier;
        out.extend_from_slice(b"\x1b[0");
        if s.add_modifier.contains(Modifier::BOLD) {
            out.extend_from_slice(b";1");
        }
        if s.add_modifier.contains(Modifier::DIM) {
            out.extend_from_slice(b";2");
        }
        if s.add_modifier.contains(Modifier::ITALIC) {
            out.extend_from_slice(b";3");
        }
        if s.add_modifier.contains(Modifier::UNDERLINED) {
            out.extend_from_slice(b";4");
        }
        if s.add_modifier.contains(Modifier::REVERSED) {
            out.extend_from_slice(b";7");
        }
        if let Some(fg) = s.fg {
            write_color(out, fg, false);
        }
        if let Some(bg) = s.bg {
            write_color(out, bg, true);
        }
        out.push(b'm');
    }

    fn write_color(out: &mut Vec<u8>, color: style::Color, is_bg: bool) {
        use style::Color;
        match color {
            Color::Reset => out.extend_from_slice(if is_bg { b";49" } else { b";39" }),
            Color::Black => out.extend_from_slice(if is_bg { b";40" } else { b";30" }),
            Color::Red => out.extend_from_slice(if is_bg { b";41" } else { b";31" }),
            Color::Green => out.extend_from_slice(if is_bg { b";42" } else { b";32" }),
            Color::Yellow => out.extend_from_slice(if is_bg { b";43" } else { b";33" }),
            Color::Blue => out.extend_from_slice(if is_bg { b";44" } else { b";34" }),
            Color::Magenta => out.extend_from_slice(if is_bg { b";45" } else { b";35" }),
            Color::Cyan => out.extend_from_slice(if is_bg { b";46" } else { b";36" }),
            Color::Gray => out.extend_from_slice(if is_bg { b";47" } else { b";37" }),
            Color::DarkGray => out.extend_from_slice(if is_bg { b";100" } else { b";90" }),
            Color::LightRed => out.extend_from_slice(if is_bg { b";101" } else { b";91" }),
            Color::LightGreen => out.extend_from_slice(if is_bg { b";102" } else { b";92" }),
            Color::LightYellow => out.extend_from_slice(if is_bg { b";103" } else { b";93" }),
            Color::LightBlue => out.extend_from_slice(if is_bg { b";104" } else { b";94" }),
            Color::LightMagenta => out.extend_from_slice(if is_bg { b";105" } else { b";95" }),
            Color::LightCyan => out.extend_from_slice(if is_bg { b";106" } else { b";96" }),
            Color::White => out.extend_from_slice(if is_bg { b";107" } else { b";97" }),
            Color::Indexed(idx) => {
                out.extend_from_slice(if is_bg { b";48;5;" } else { b";38;5;" });
                push_u16(out, u16::from(idx));
            }
            Color::Rgb(r, g, b) => {
                out.extend_from_slice(if is_bg { b";48;2;" } else { b";38;2;" });
                push_u16(out, u16::from(r));
                out.push(b';');
                push_u16(out, u16::from(g));
                out.push(b';');
                push_u16(out, u16::from(b));
            }
        }
    }

    fn push_u16(out: &mut Vec<u8>, mut n: u16) {
        if n == 0 {
            out.push(b'0');
            return;
        }
        let mut digits = [0u8; 5];
        let mut len = 0;
        while n > 0 {
            digits[len] = b'0' + (n % 10) as u8;
            n /= 10;
            len += 1;
        }
        while len > 0 {
            len -= 1;
            out.push(digits[len]);
        }
    }

    /// UAX #11 terminal cell width for `ch`.
    pub(crate) fn char_width(ch: char) -> usize {
        let u = ch as u32;
        if u < 0x7f {
            return usize::from(u >= 0x20);
        }
        if (0x7f..0xa0).contains(&u) || is_zero_width(u) {
            return 0;
        }
        if is_wide(u) {
            return 2;
        }
        1
    }

    fn is_zero_width(u: u32) -> bool {
        matches!(
            u,
            0x00ad
                | 0x0300..=0x036f
                | 0x0483..=0x0489
                | 0x0591..=0x05bd
                | 0x05bf
                | 0x05c1..=0x05c2
                | 0x05c4..=0x05c5
                | 0x05c7
                | 0x0600..=0x0605
                | 0x0610..=0x061a
                | 0x064b..=0x065f
                | 0x0670
                | 0x06d6..=0x06dd
                | 0x06df..=0x06e4
                | 0x06e7..=0x06e8
                | 0x06ea..=0x06ed
                | 0x070f
                | 0x0711
                | 0x0730..=0x074a
                | 0x200b..=0x200f
                | 0x202a..=0x202e
                | 0x2060..=0x2064
                | 0xfe00..=0xfe0f
                | 0xfe20..=0xfe2f
                | 0xfeff
                | 0xfff9..=0xfffb
                | 0xe0100..=0xe01ef
        )
    }

    fn is_wide(u: u32) -> bool {
        matches!(
            u,
            0x1100..=0x115f
                | 0x231a..=0x231b
                | 0x2329..=0x232a
                | 0x23e9..=0x23ec
                | 0x23f0
                | 0x23f3
                | 0x25fd..=0x25fe
                | 0x2614..=0x2615
                | 0x2648..=0x2653
                | 0x267f
                | 0x2693
                | 0x26a1
                | 0x26aa..=0x26ab
                | 0x26bd..=0x26be
                | 0x26c4..=0x26c5
                | 0x26ce
                | 0x26d4
                | 0x26ea
                | 0x26f2..=0x26f3
                | 0x26f5
                | 0x26fa
                | 0x26fd
                | 0x2705
                | 0x270a..=0x270b
                | 0x2728
                | 0x274c
                | 0x274e
                | 0x2753..=0x2755
                | 0x2757
                | 0x2795..=0x2797
                | 0x27b0
                | 0x27bf
                | 0x2b1b..=0x2b1c
                | 0x2b50
                | 0x2b55
                | 0x2e80..=0x303e
                | 0x3041..=0x33bf
                | 0x3400..=0x4dbf
                | 0x4e00..=0xa4cf
                | 0xa960..=0xa97c
                | 0xac00..=0xd7a3
                | 0xf900..=0xfaff
                | 0xfe10..=0xfe19
                | 0xfe30..=0xfe6b
                | 0xff01..=0xff60
                | 0xffe0..=0xffe6
                | 0x1f004
                | 0x1f0cf
                | 0x1f18e
                | 0x1f191..=0x1f19a
                | 0x1f200..=0x1f251
                | 0x1f300..=0x1f64f
                | 0x1f680..=0x1f6ff
                | 0x1f900..=0x1f9ff
                | 0x1fa70..=0x1faff
                | 0x20000..=0x2fffd
                | 0x30000..=0x3fffd
        )
    }
}

#[cfg(test)]
mod tests {
    use super::render::char_width;
    use super::render::layout::{Constraint, Layout, Rect};
    use super::term::event::{
        Event, KeyCode, KeyModifiers, MouseButton, MouseEventKind, decode_events,
    };
    use std::collections::VecDeque;

    #[test]
    fn key_and_sgr_mouse_decoding() {
        let mut buf = b"q\x04\x1b[A\x1b[5~\x1b[<0;10;5M\x1b[<64;3;2M".to_vec();
        let mut out = VecDeque::new();
        decode_events(&mut buf, &mut out, true);
        assert!(buf.is_empty());

        let events: Vec<Event> = out.into_iter().collect();
        assert!(matches!(
            events[0],
            Event::Key(k) if k.code == KeyCode::Char('q') && k.modifiers == KeyModifiers::NONE
        ));
        assert!(matches!(
            events[1],
            Event::Key(k) if k.code == KeyCode::Char('d') && k.modifiers == KeyModifiers::CONTROL
        ));
        assert!(matches!(events[2], Event::Key(k) if k.code == KeyCode::Up));
        assert!(matches!(events[3], Event::Key(k) if k.code == KeyCode::PageUp));
        assert!(matches!(
            events[4],
            Event::Mouse(m) if m.kind == MouseEventKind::Down(MouseButton::Left) && m.column == 9 && m.row == 4
        ));
        assert!(matches!(
            events[5],
            Event::Mouse(m) if m.kind == MouseEventKind::ScrollUp && m.column == 2 && m.row == 1
        ));
    }

    #[test]
    fn cell_widths_match_ui_badges_and_gutters() {
        for g in "│╭╰╮╯─►◄↓▼▲═▏▸█·≡↳≣▾φᵀ".chars() {
            assert_eq!(char_width(g), 1, "{g:?}");
        }
        assert_eq!(char_width('⚡'), 2);
        assert_eq!(char_width('❌'), 2);
    }

    #[test]
    fn layout_survives_tiny_viewport() {
        let [top, mid, bot] = Layout::vertical([
            Constraint::Length(1),
            Constraint::Min(1),
            Constraint::Length(1),
        ])
        .areas(Rect::new(0, 0, 40, 1));
        assert_eq!(top.height + mid.height + bot.height, 1);
    }

    #[cfg(unix)]
    #[test]
    fn read_eof_with_incomplete_escape_does_not_spin() {
        use super::term::event::{ReaderState, read_from};
        use std::io;
        use std::sync::{Mutex, mpsc};
        use std::time::Duration;

        fn run_pipe(bytes: &[u8]) -> (io::Result<Event>, io::Result<Event>) {
            let mut fds = [0i32; 2];
            assert_eq!(unsafe { libc::pipe(fds.as_mut_ptr()) }, 0);
            assert_eq!(
                unsafe { libc::write(fds[1], bytes.as_ptr().cast(), bytes.len()) },
                bytes.len() as isize
            );
            unsafe {
                libc::close(fds[1]);
            }

            let read_fd = fds[0];
            let (tx, rx) = mpsc::channel();
            let handle = std::thread::spawn(move || {
                let reader = Mutex::new(ReaderState::new());
                let first = read_from(read_fd, &reader);
                let second = read_from(read_fd, &reader);
                let _ = tx.send((first, second));
            });

            let result = rx.recv_timeout(Duration::from_millis(500));
            unsafe {
                libc::close(read_fd);
            }
            let _ = handle.join();
            result.expect("term::event::read spun on EOF with buffered incomplete escape sequence")
        }

        let (first, second) = run_pipe(b"\x1b");
        let ev = first.expect("buffered bare ESC should flush before EOF");
        assert!(matches!(
            ev,
            Event::Key(k) if k.code == KeyCode::Esc && k.modifiers == KeyModifiers::NONE
        ));
        let err = second.expect_err("subsequent read at EOF should return UnexpectedEof");
        assert_eq!(err.kind(), io::ErrorKind::UnexpectedEof);

        let (first_csi, _) = run_pipe(b"\x1b[");
        let err_csi = first_csi.expect_err("incomplete CSI at EOF should return UnexpectedEof");
        assert_eq!(err_csi.kind(), io::ErrorKind::UnexpectedEof);

        let (first_utf8, _) = run_pipe(b"\xe2\x82");
        let err_utf8 = first_utf8.expect_err("incomplete UTF-8 at EOF should return UnexpectedEof");
        assert_eq!(err_utf8.kind(), io::ErrorKind::UnexpectedEof);
    }
}
