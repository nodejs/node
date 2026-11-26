// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Clipboard and export.
//!
//! Copy strategy per PLAN §7.9: over SSH, the only thing that can reach the
//! *local* clipboard is OSC 52 — an escape sequence the terminal emulator
//! itself interprets — so that is tried there. Locally, standard OS clipboard
//! helpers (`pbcopy` on macOS; `wl-copy` / `xclip` / `xsel` on Linux) talk to
//! the OS clipboard directly, falling back to OSC 52 when unavailable.

use std::io::Write;
use std::process::{Command, Stdio};

use anyhow::{Context, Result};
use base64_engine as b64;

use crate::tracing;

/// Hand-rolled standard base64: the payload of OSC 52. Ten lines beat a
/// dependency for exactly one call site.
mod base64_engine {
    const TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    pub fn encode(data: &[u8]) -> String {
        let mut out = String::with_capacity(data.len().div_ceil(3) * 4);
        for chunk in data.chunks(3) {
            let b = [
                chunk[0],
                chunk.get(1).copied().unwrap_or(0),
                chunk.get(2).copied().unwrap_or(0),
            ];
            let n = u32::from_be_bytes([0, b[0], b[1], b[2]]);
            out.push(TABLE[(n >> 18 & 63) as usize] as char);
            out.push(TABLE[(n >> 12 & 63) as usize] as char);
            out.push(if chunk.len() > 1 {
                TABLE[(n >> 6 & 63) as usize] as char
            } else {
                '='
            });
            out.push(if chunk.len() > 2 {
                TABLE[(n & 63) as usize] as char
            } else {
                '='
            });
        }
        out
    }
}

/// Some terminals silently drop oversized OSC 52 payloads (tmux's default cap
/// is 100 KB *encoded*; 72 KiB raw ≈ 96 KB in base64 plus the escape
/// overhead); past this the OSC 52 route refuses with a message rather than
/// silently truncating. **OSC 52 only** — the OS clipboard has no such cap.
pub const MAX_COPY: usize = 72 * 1024;

/// Local copies go through the OS clipboard, which takes megabytes without
/// complaint; this bound only stops a yank from materialising an absurd
/// section into memory first.
pub const MAX_LOCAL_COPY: usize = 64 * 1024 * 1024;

/// The size the current environment's clipboard route can accept — what
/// callers should check *before* materialising the text.
pub fn max_copy() -> usize {
    if remote() { MAX_COPY } else { MAX_LOCAL_COPY }
}

/// Is this session one where only OSC 52 can possibly reach the user's
/// clipboard? **SSH only.**
fn remote() -> bool {
    std::env::var_os("SSH_TTY").is_some() || std::env::var_os("SSH_CONNECTION").is_some()
}

/// Copies text to the clipboard. Returns a short description of the route
/// taken, for the status line.
pub fn copy(text: &str) -> Result<&'static str> {
    if remote() {
        osc52(text)?;
        return Ok("copied (OSC 52)");
    }

    match copy_os_clipboard(text) {
        Ok(()) => Ok("copied"),
        Err(e) => {
            tracing::debug!("OS clipboard failed ({e}); falling back to OSC 52");
            osc52(text)?;
            Ok("copied (OSC 52)")
        }
    }
}

fn copy_os_clipboard(text: &str) -> Result<()> {
    #[cfg(target_os = "macos")]
    {
        return pipe_to_cmd("pbcopy", &[], text);
    }

    #[cfg(not(target_os = "macos"))]
    {
        let candidates: &[(&str, &[&str])] = &[
            ("wl-copy", &[]),
            ("xclip", &["-selection", "clipboard"]),
            ("xsel", &["--clipboard", "--input"]),
        ];
        let mut last_err = anyhow::anyhow!("no clipboard utility available");
        for (prog, args) in candidates {
            match pipe_to_cmd(prog, args, text) {
                Ok(()) => return Ok(()),
                Err(e) => last_err = e,
            }
        }
        Err(last_err)
    }
}

fn pipe_to_cmd(prog: &str, args: &[&str], text: &str) -> Result<()> {
    let mut child = Command::new(prog)
        .args(args)
        .stdin(Stdio::piped())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .with_context(|| format!("cannot spawn {prog}"))?;
    if let Some(mut stdin) = child.stdin.take() {
        stdin.write_all(text.as_bytes())?;
    }
    let status = child.wait()?;
    if !status.success() {
        anyhow::bail!("{prog} exited with {status}");
    }
    Ok(())
}

/// Emits the OSC 52 sequence on the terminal, written to the same screen
/// handle the TUI draws on.
fn osc52(text: &str) -> Result<()> {
    if text.len() > MAX_COPY {
        anyhow::bail!(
            "selection is {} KB; OSC 52 tops out at {} KB — use export instead",
            text.len() / 1024,
            MAX_COPY / 1024
        );
    }
    let payload = b64::encode(text.as_bytes());
    let mut screen = crate::tty::Screen::open().context("no terminal for OSC 52")?;

    if std::env::var_os("TMUX").is_some() && tmux_allows_passthrough() {
        let inner = format!("\x1b]52;c;{payload}\x07");
        let wrapped = format!("\x1bPtmux;{}\x1b\\", inner.replace('\x1b', "\x1b\x1b"));
        screen.write_all(wrapped.as_bytes())?;
    } else {
        write!(screen, "\x1b]52;c;{payload}\x07")?;
    }
    screen.flush()?;
    Ok(())
}

fn tmux_allows_passthrough() -> bool {
    Command::new("tmux")
        .args(["show", "-Ap", "allow-passthrough"])
        .output()
        .map(|out| {
            let text = String::from_utf8_lossy(&out.stdout);
            text.contains(" on") || text.contains("all")
        })
        .unwrap_or(false)
}

/// Writes the current view as Markdown: a fenced block with a
/// one-line provenance header, ready to paste into a bug or Gerrit comment.
pub fn export(path: &std::path::Path, title: &str, lines: &[String]) -> Result<()> {
    let mut out = String::with_capacity(lines.iter().map(|l| l.len() + 1).sum::<usize>() + 128);
    out.push_str(&format!("### {title}\n\n```\n"));
    for line in lines {
        out.push_str(line);
        out.push('\n');
    }
    out.push_str("```\n");
    std::fs::write(path, out).with_context(|| format!("cannot write {}", path.display()))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn base64_matches_the_rfc() {
        assert_eq!(b64::encode(b""), "");
        assert_eq!(b64::encode(b"f"), "Zg==");
        assert_eq!(b64::encode(b"fo"), "Zm8=");
        assert_eq!(b64::encode(b"foo"), "Zm9v");
        assert_eq!(b64::encode(b"foob"), "Zm9vYg==");
        assert_eq!(b64::encode(b"fooba"), "Zm9vYmE=");
        assert_eq!(b64::encode(b"foobar"), "Zm9vYmFy");
    }

    #[test]
    fn export_writes_fenced_markdown() {
        let dir = std::env::temp_dir().join(format!("garage-test-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("export.md");
        export(
            &path,
            "add · Maglev #1 · Register allocation",
            &["  9/9: CheckedSmiUntag".to_string()],
        )
        .unwrap();
        let text = std::fs::read_to_string(&path).unwrap();
        assert!(text.starts_with("### add · Maglev #1"));
        assert!(text.contains("```\n  9/9: CheckedSmiUntag\n```"));
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
