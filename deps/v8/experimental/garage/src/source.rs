// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Input abstraction and reader threads.
//!
//! Two ways bytes arrive, and they are genuinely different:
//!
//! - **File**: memory-mapped. The whole thing is addressable immediately, with
//!   no copy and no read loop, which is what makes "open a 1 GB trace instantly"
//!   (PLAN §3.3) achievable at all.
//! - **Stdin**: streamed in chunks, arriving over the life of the session.
//!
//! `Process` (wrapper mode) is deliberately absent; the enum has
//! room for it and nothing here assumes there are only two variants.
//!
//! Both feed the UI through one channel. Plain threads, no async runtime —
//! PLAN §11 says adopt `tokio` only if wrapper-mode process management actually
//! needs it, and one reader per source does not.

use std::fmt;
use std::fs::File;
use std::io::Read;
use std::ops::Deref;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::mpsc::Sender;
use std::thread;

use crate::tracing;

/// Read-only memory mapping over an open file descriptor.
pub struct Mmap {
    ptr: *const u8,
    len: usize,
}

// SAFETY: the mapping is read-only (`PROT_READ`) and lives until `Drop`.
unsafe impl Send for Mmap {}
unsafe impl Sync for Mmap {}

impl Mmap {
    #[cfg(unix)]
    pub unsafe fn map(file: &File) -> std::io::Result<Self> {
        use std::os::fd::AsRawFd;
        let meta = file.metadata()?;
        let len = usize::try_from(meta.len()).map_err(|_| {
            std::io::Error::new(std::io::ErrorKind::InvalidInput, "file too large to map")
        })?;
        if len == 0 {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                "cannot memory-map zero-length file",
            ));
        }
        let raw = unsafe {
            libc::mmap(
                std::ptr::null_mut(),
                len,
                libc::PROT_READ,
                libc::MAP_PRIVATE,
                file.as_raw_fd(),
                0,
            )
        };
        if raw == libc::MAP_FAILED {
            return Err(std::io::Error::last_os_error());
        }
        Ok(Self {
            ptr: raw.cast::<u8>(),
            len,
        })
    }

    #[cfg(not(unix))]
    pub unsafe fn map(_file: &File) -> std::io::Result<Self> {
        Err(std::io::Error::new(
            std::io::ErrorKind::Unsupported,
            "mmap requires unix",
        ))
    }
}

impl Deref for Mmap {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        unsafe { std::slice::from_raw_parts(self.ptr, self.len) }
    }
}

impl Drop for Mmap {
    fn drop(&mut self) {
        #[cfg(unix)]
        if self.len > 0 {
            unsafe {
                let _ = libc::munmap(self.ptr.cast_mut().cast(), self.len);
            }
        }
    }
}

/// 64 KiB: large enough that the channel is not the bottleneck on a fast pipe,
/// small enough that the first screenful shows up immediately.
const CHUNK: usize = 64 * 1024;

#[derive(Debug, Clone)]
pub enum LogSource {
    File(PathBuf),
    Stdin,
    /// Wrapper mode: `garage -- d8 …` spawns the command and
    /// streams its output live.
    Command(Vec<String>),
}

impl LogSource {
    pub fn label(&self) -> String {
        match self {
            LogSource::File(p) => p.display().to_string(),
            LogSource::Stdin => "<stdin>".to_string(),
            LogSource::Command(argv) => argv.join(" "),
        }
    }
}

/// The wrapped child's pid, for cleanup on quit and from the signal
/// handler. Zero = no child.
pub static CHILD_PID: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(0);

/// Terminates the wrapped child, if one is running. Safe to call twice;
/// also called from the signal handler (kill(2) is async-signal-safe).
pub fn kill_child() {
    let pid = CHILD_PID.swap(0, std::sync::atomic::Ordering::SeqCst);
    if pid != 0 {
        unsafe {
            libc::kill(pid as libc::pid_t, libc::SIGTERM);
        }
    }
}

/// Bytes and lifecycle transitions, tagged with the index of the source they
/// came from.
pub enum SourceEvent {
    /// A whole file, mapped. Arrives once, immediately.
    Mapped { source: usize, map: Arc<Mmap> },
    /// Streamed bytes.
    Chunk { source: usize, bytes: Vec<u8> },
    /// No more bytes will arrive from this source.
    Eof { source: usize },
    /// The source failed. Reported in the UI, never fatal to the session:
    /// one unreadable file must not take down a multi-file run.
    Failed { source: usize, error: String },
}

#[derive(Debug)]
pub enum SourceError {
    Open {
        path: PathBuf,
        source: std::io::Error,
    },
    Map {
        path: PathBuf,
        source: std::io::Error,
    },
    Stdin(std::io::Error),
}

impl fmt::Display for SourceError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SourceError::Open { path, source } => write!(f, "{}: {source}", path.display()),
            SourceError::Map { path, source } => {
                write!(f, "{}: cannot memory-map: {source}", path.display())
            }
            SourceError::Stdin(err) => write!(f, "stdin: {err}"),
        }
    }
}

impl std::error::Error for SourceError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            SourceError::Open { source, .. }
            | SourceError::Map { source, .. }
            | SourceError::Stdin(source) => Some(source),
        }
    }
}

/// Starts one reader thread per source.
///
/// Threads are detached: they end at EOF, or when the receiver is dropped and
/// `send` starts failing. Neither needs joining, and making quit wait on a
/// blocked `read` from a pipe that will never close is exactly the hang we do
/// not want.
///
/// `stdin` is the descriptor displaced by [`crate::tty::acquire`]. Once fd 0
/// has been repointed at the terminal, `io::stdin()` is the *keyboard*, so
/// reading the trace from it would consume keystrokes and show the user their
/// own typing as trace data.
pub fn spawn_readers(
    sources: &[LogSource],
    mut stdin: Option<File>,
    tx: Sender<crate::event::Event>,
) {
    for (index, source) in sources.iter().enumerate() {
        let reader_tx = tx.clone();
        let source = source.clone();
        let stdin = if matches!(source, LogSource::Stdin) {
            stdin.take()
        } else {
            None
        };
        let spawned = thread::Builder::new()
            .name(format!("garage-reader-{index}"))
            .spawn(move || read_source(index, source, stdin, reader_tx));

        if let Err(e) = spawned {
            // Cannot spawn a thread: report it through the same channel the
            // reader would have used, so it surfaces in the UI like any other
            // source failure instead of vanishing.
            tracing::error!("cannot start reader thread: {e}");
            let _ = tx.send(crate::event::Event::Source(SourceEvent::Failed {
                source: index,
                error: format!("cannot start reader thread: {e}"),
            }));
        }
    }
}

fn read_source(
    index: usize,
    source: LogSource,
    stdin: Option<File>,
    tx: Sender<crate::event::Event>,
) {
    let result = match &source {
        LogSource::File(path) => read_file(index, path, &tx),
        LogSource::Stdin => match stdin {
            Some(file) => read_stream(index, file, &tx),
            None => Err(SourceError::Stdin(std::io::Error::other(
                "stdin was not captured before the terminal took fd 0",
            ))),
        },
        LogSource::Command(argv) => read_command(index, argv, &tx),
    };

    let event = match result {
        Ok(()) => SourceEvent::Eof { source: index },
        Err(e) => {
            tracing::error!(source = %source.label(), "read failed: {e}");
            SourceEvent::Failed {
                source: index,
                error: e.to_string(),
            }
        }
    };
    let _ = tx.send(crate::event::Event::Source(event));
}

fn read_file(
    index: usize,
    path: &Path,
    tx: &Sender<crate::event::Event>,
) -> Result<(), SourceError> {
    let file = File::open(path).map_err(|source| SourceError::Open {
        path: path.to_path_buf(),
        source,
    })?;

    let len = file
        .metadata()
        .map_err(|source| SourceError::Open {
            path: path.to_path_buf(),
            source,
        })?
        .len();

    // memmap2 refuses a zero-length mapping, and an empty trace file is a
    // perfectly ordinary thing to be handed (`d8 ... > x.log` that produced
    // nothing). Report EOF rather than an error.
    if len == 0 {
        tracing::debug!(path = %path.display(), "empty file");
        return Ok(());
    }

    // SAFETY: the usual mmap caveat — if another process truncates or rewrites
    // this file while we hold the mapping, reads can fault or observe torn
    // data. garage is a viewer of files that are, in practice, finished being
    // written; the alternative (copying a gigabyte into RAM) defeats the point.
    let map = unsafe { Mmap::map(&file) }.map_err(|source| SourceError::Map {
        path: path.to_path_buf(),
        source,
    })?;

    tracing::debug!(path = %path.display(), bytes = map.len(), "mapped");
    let _ = tx.send(crate::event::Event::Source(SourceEvent::Mapped {
        source: index,
        map: Arc::new(map),
    }));
    Ok(())
}

/// Wrapper mode: spawn the command with stdout and stderr both
/// piped, and merge the two streams into one source in arrival order — the
/// same interleaving a terminal would show, and the only merge that never
/// stalls one stream behind the other. d8 prints traces on stdout and
/// crashes on stderr; both belong in the session.
fn read_command(
    index: usize,
    argv: &[String],
    tx: &Sender<crate::event::Event>,
) -> Result<(), SourceError> {
    use std::process::{Command, Stdio};
    let (program, args) = argv
        .split_first()
        .ok_or_else(|| SourceError::Stdin(std::io::Error::other("empty command")))?;
    let mut child = Command::new(program)
        .args(args)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| {
            SourceError::Stdin(std::io::Error::new(
                e.kind(),
                format!("cannot spawn {program}: {e}"),
            ))
        })?;
    CHILD_PID.store(child.id(), std::sync::atomic::Ordering::SeqCst);

    let stdout = child.stdout.take();
    let stderr = child.stderr.take();
    let err_thread = stderr.map(|err| {
        let tx = tx.clone();
        thread::spawn(move || {
            let _ = read_stream(index, err, &tx);
        })
    });
    if let Some(out) = stdout {
        read_stream(index, out, tx)?;
    }
    if let Some(t) = err_thread {
        let _ = t.join();
    }
    let status = child.wait();
    CHILD_PID.store(0, std::sync::atomic::Ordering::SeqCst);
    match status {
        Ok(st) if !st.success() => {
            // Surface the exit code as trace content, where it is visible
            // next to whatever the child printed last.
            let note = format!("\n[garage] child exited with {st}\n");
            let _ = tx.send(crate::event::Event::Source(SourceEvent::Chunk {
                source: index,
                bytes: note.into_bytes(),
            }));
        }
        _ => {}
    }
    Ok(())
}

fn read_stream(
    index: usize,
    mut stream: impl Read,
    tx: &Sender<crate::event::Event>,
) -> Result<(), SourceError> {
    let mut buf = vec![0u8; CHUNK];

    loop {
        let n = stream.read(&mut buf).map_err(SourceError::Stdin)?;
        if n == 0 {
            return Ok(());
        }
        let event = crate::event::Event::Source(SourceEvent::Chunk {
            source: index,
            bytes: buf[..n].to_vec(),
        });
        // A send error means the UI is gone; stop reading rather than spin.
        if tx.send(event).is_err() {
            return Ok(());
        }
    }
}

/// Where a source's bytes live once they have arrived.
enum Storage {
    /// Borrowed from the OS. No copy was made.
    Mapped(Arc<Mmap>),
    /// Accumulated from a stream.
    Owned(Vec<u8>),
}

/// Raw bytes plus a line index.
///
/// The bytes are kept exactly as they arrived, escapes and all: the raw view
/// needs d8's own colours (Phase 4.1) and the parser needs byte offsets that
/// still refer to the file on disk. Stripping happens at display time.
pub struct LogBuffer {
    storage: Storage,
    /// Byte offset of the first byte of each line. Offsets are `<= len()`;
    /// while a stream is live, an offset *equal* to `len()` can exist and
    /// means "a line whose bytes have not arrived yet" — `line_count()`
    /// includes it as a phantom empty line until [`LogBuffer::finish`] pops
    /// it at EOF. Consumers that must not see the phantom hold the last line
    /// back until EOF, as the indexer does.
    line_starts: Vec<usize>,
    /// How far the line index has been built.
    scanned: usize,
}

impl Default for LogBuffer {
    fn default() -> Self {
        Self::new()
    }
}

impl LogBuffer {
    pub fn new() -> Self {
        Self {
            storage: Storage::Owned(Vec::new()),
            line_starts: Vec::new(),
            scanned: 0,
        }
    }

    pub fn bytes(&self) -> &[u8] {
        match &self.storage {
            Storage::Mapped(m) => m,
            Storage::Owned(v) => v,
        }
    }

    pub fn len(&self) -> usize {
        self.bytes().len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn line_count(&self) -> usize {
        self.line_starts.len()
    }

    /// Replaces the contents with a mapping. Indexing the whole file happens
    /// here, once.
    pub fn adopt_map(&mut self, map: Arc<Mmap>) {
        let started = std::time::Instant::now();
        self.storage = Storage::Mapped(map);
        self.line_starts.clear();
        self.scanned = 0;
        self.index_new_bytes();
        // The one O(file size) step on the open path, so it is the number that
        // decides whether "open 500 MB in under 2 s" (PLAN §12) holds.
        tracing::debug!(
            bytes = self.len(),
            lines = self.line_count(),
            micros = started.elapsed().as_micros(),
            "line index built"
        );
    }

    /// Appends streamed bytes and extends the line index over them.
    pub fn append(&mut self, bytes: &[u8]) {
        match &mut self.storage {
            Storage::Owned(v) => v.extend_from_slice(bytes),
            // A source is either mapped or streamed, never both.
            Storage::Mapped(_) => {
                tracing::warn!("ignoring streamed bytes on a mapped source");
                return;
            }
        }
        self.index_new_bytes();
    }

    /// Called at EOF. Drops the phantom empty line that a trailing newline
    /// leaves behind: while streaming, "a start at the end of the buffer" means
    /// "a line whose bytes have not arrived yet", but at EOF it means nothing.
    pub fn finish(&mut self) {
        if self.line_starts.last() == Some(&self.len()) {
            self.line_starts.pop();
        }
    }

    fn index_new_bytes(&mut self) {
        let bytes = match &self.storage {
            Storage::Mapped(m) => &m[..],
            Storage::Owned(v) => &v[..],
        };
        if self.line_starts.is_empty() && !bytes.is_empty() {
            self.line_starts.push(0);
        }
        for (offset, byte) in bytes.iter().enumerate().skip(self.scanned) {
            if *byte == b'\n' {
                self.line_starts.push(offset + 1);
            }
        }
        self.scanned = bytes.len();
    }

    /// Total bytes spanned by a line range — O(1) from the offset index.
    /// The size answer for "can this even be copied?" without materialising
    /// a single line.
    pub fn span_bytes(&self, lines: std::ops::Range<usize>) -> usize {
        if lines.is_empty() {
            return 0;
        }
        let start = self
            .line_starts
            .get(lines.start)
            .copied()
            .unwrap_or(self.len());
        let end = self
            .line_starts
            .get(lines.end)
            .copied()
            .unwrap_or(self.len());
        end.saturating_sub(start)
    }

    /// The bytes of line `i`, without its terminator.
    pub fn line(&self, i: usize) -> Option<&[u8]> {
        let start = *self.line_starts.get(i)?;
        let end = self.line_starts.get(i + 1).copied().unwrap_or(self.len());

        let mut line = &self.bytes()[start..end];
        if line.last() == Some(&b'\n') {
            line = &line[..line.len() - 1];
        }
        if line.last() == Some(&b'\r') {
            line = &line[..line.len() - 1];
        }
        Some(line)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn streamed(chunks: &[&[u8]]) -> LogBuffer {
        let mut b = LogBuffer::new();
        for c in chunks {
            b.append(c);
        }
        b.finish();
        b
    }

    #[test]
    fn empty_buffer_has_no_lines() {
        let b = streamed(&[]);
        assert_eq!(b.line_count(), 0);
        assert!(b.line(0).is_none());
    }

    #[test]
    fn trailing_newline_does_not_add_a_line() {
        let b = streamed(&[b"a\nb\n"]);
        assert_eq!(b.line_count(), 2);
        assert_eq!(b.line(0).unwrap(), b"a");
        assert_eq!(b.line(1).unwrap(), b"b");
    }

    #[test]
    fn missing_trailing_newline_still_yields_the_last_line() {
        let b = streamed(&[b"a\nb"]);
        assert_eq!(b.line_count(), 2);
        assert_eq!(b.line(1).unwrap(), b"b");
    }

    #[test]
    fn blank_lines_are_preserved() {
        let b = streamed(&[b"a\n\nb\n"]);
        assert_eq!(b.line_count(), 3);
        assert_eq!(b.line(1).unwrap(), b"");
    }

    /// The case that makes incremental indexing tricky: a line split across two
    /// chunk boundaries must not become two lines.
    #[test]
    fn chunk_boundaries_do_not_split_lines() {
        let b = streamed(&[b"ab", b"cd\ne", b"f\n"]);
        assert_eq!(b.line_count(), 2);
        assert_eq!(b.line(0).unwrap(), b"abcd");
        assert_eq!(b.line(1).unwrap(), b"ef");
    }

    /// A newline landing exactly on a chunk boundary used to leave a phantom
    /// empty line if `finish` was not called.
    #[test]
    fn newline_at_chunk_boundary() {
        let b = streamed(&[b"a\n", b"b\n"]);
        assert_eq!(b.line_count(), 2);
    }

    #[test]
    fn partial_last_line_is_visible_while_streaming() {
        // No finish(): mid-stream, a line whose newline has not arrived yet is
        // still worth showing.
        let mut b = LogBuffer::new();
        b.append(b"done\npartial");
        assert_eq!(b.line_count(), 2);
        assert_eq!(b.line(1).unwrap(), b"partial");
    }

    #[test]
    fn crlf_is_trimmed() {
        let b = streamed(&[b"a\r\nb\r\n"]);
        assert_eq!(b.line(0).unwrap(), b"a");
    }

    #[test]
    fn bytes_are_kept_verbatim() {
        // Escapes stay in the buffer; only the display path strips them.
        let b = streamed(&[b"\x1b[0;34ma\x1b[0m\n"]);
        assert_eq!(b.line(0).unwrap(), b"\x1b[0;34ma\x1b[0m");
    }
}
