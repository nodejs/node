// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! File-based diagnostics and structured logging macros.
//!
//! Nothing may write to stdout or stderr while the alternate screen is up, so
//! diagnostics go to a file and only to a file. The logger is installed only
//! when asked for — `--debug`, or `GARAGE_LOG=<filter>` — so a normal run does
//! not drop a `garage.log` into whatever directory the user happened to be in.

use std::fs::File;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;
use std::time::{SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Level {
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4,
    Trace = 5,
}

impl Level {
    fn as_str(self) -> &'static str {
        match self {
            Level::Error => "ERROR",
            Level::Warn => "WARN ",
            Level::Info => "INFO ",
            Level::Debug => "DEBUG",
            Level::Trace => "TRACE",
        }
    }

    fn parse(s: &str) -> Option<Self> {
        match s.trim().to_ascii_lowercase().as_str() {
            "error" => Some(Level::Error),
            "warn" | "warning" => Some(Level::Warn),
            "info" => Some(Level::Info),
            "debug" => Some(Level::Debug),
            "trace" => Some(Level::Trace),
            _ => None,
        }
    }
}

struct FilterRule {
    prefix: Option<String>,
    level: Option<Level>,
}

struct Logger {
    file: File,
    default_level: Option<Level>,
    rules: Vec<FilterRule>,
}

impl Logger {
    fn enabled(&self, level: Level, target: &str) -> bool {
        let mut matched: Option<Option<Level>> = None;
        let mut best_len = 0usize;
        for r in &self.rules {
            if let Some(p) = &r.prefix
                && (target == p || target.starts_with(&format!("{p}::")))
                && p.len() >= best_len
            {
                best_len = p.len();
                matched = Some(r.level);
            }
        }
        let max_level = matched.unwrap_or(self.default_level);
        max_level.is_some_and(|max| level <= max)
    }
}

static LOGGER: OnceLock<Logger> = OnceLock::new();

fn parse_filter(spec: &str) -> (Option<Level>, Vec<FilterRule>) {
    let mut default_level = Some(Level::Error);
    let mut rules = Vec::new();
    for directive in spec.split(',').map(str::trim).filter(|s| !s.is_empty()) {
        if let Some((target, lvl)) = directive.split_once('=') {
            let level = if lvl.eq_ignore_ascii_case("off") {
                None
            } else {
                Level::parse(lvl).or(Some(Level::Trace))
            };
            rules.push(FilterRule {
                prefix: Some(target.trim().to_string()),
                level,
            });
        } else if directive.eq_ignore_ascii_case("off") {
            default_level = None;
        } else if let Some(lvl) = Level::parse(directive) {
            default_level = Some(lvl);
        } else {
            rules.push(FilterRule {
                prefix: Some(directive.to_string()),
                level: Some(Level::Trace),
            });
        }
    }
    (default_level, rules)
}

#[doc(hidden)]
pub fn is_enabled(level: Level, target: &str) -> bool {
    LOGGER.get().is_some_and(|l| l.enabled(level, target))
}

#[doc(hidden)]
pub fn emit(level: Level, target: &str, fields: &str, message: std::fmt::Arguments<'_>) {
    let Some(logger) = LOGGER.get() else {
        return;
    };
    if !logger.enabled(level, target) {
        return;
    }
    let ts_ms = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis())
        .unwrap_or(0);
    let line = if fields.is_empty() {
        format!("{ts_ms} {} {target}: {message}\n", level.as_str())
    } else {
        format!("{ts_ms} {} {target}: {message} {fields}\n", level.as_str())
    };
    let mut file = &logger.file;
    let _ = file.write_all(line.as_bytes());
}

/// Installs the file logger if diagnostics were requested.
///
/// Returns the log path when one was opened, so the caller can tell the user
/// where to look.
pub fn init(path: &Path, debug: bool) -> Result<Option<PathBuf>> {
    let spec = match (debug, std::env::var("GARAGE_LOG")) {
        (_, Ok(spec)) if !spec.is_empty() => spec,
        (true, _) => "garage=debug".to_string(),
        (false, _) => return Ok(None),
    };

    let file = File::options()
        .create(true)
        .append(true)
        .open(path)
        .with_context(|| format!("cannot open log file {}", path.display()))?;

    let (default_level, rules) = parse_filter(&spec);
    let _ = LOGGER.set(Logger {
        file,
        default_level,
        rules,
    });

    crate::tracing::info!(version = env!("CARGO_PKG_VERSION"), "garage started");
    Ok(Some(path.to_path_buf()))
}

/// Logs a panic payload, for the panic hook.
pub fn record_panic(info: &std::panic::PanicHookInfo<'_>) {
    let message = payload(info);
    match info.location() {
        Some(loc) => crate::tracing::error!(location = %loc, "panic: {message}"),
        None => crate::tracing::error!("panic: {message}"),
    }
}

fn payload(info: &std::panic::PanicHookInfo<'_>) -> String {
    if let Some(s) = info.payload().downcast_ref::<&str>() {
        (*s).to_string()
    } else if let Some(s) = info.payload().downcast_ref::<String>() {
        s.clone()
    } else {
        "<non-string panic payload>".to_string()
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! __garage_log {
    (@step ($lvl:expr, $target:expr, $out:ident) $k:ident = %$v:expr, $($rest:tt)+) => {{
        use ::std::fmt::Write as _;
        if !$out.is_empty() { $out.push(' '); }
        let _ = write!($out, "{}={}", stringify!($k), $v);
        $crate::__garage_log!(@step ($lvl, $target, $out) $($rest)+)
    }};
    (@step ($lvl:expr, $target:expr, $out:ident) $k:ident = ?$v:expr, $($rest:tt)+) => {{
        use ::std::fmt::Write as _;
        if !$out.is_empty() { $out.push(' '); }
        let _ = write!($out, "{}={:?}", stringify!($k), $v);
        $crate::__garage_log!(@step ($lvl, $target, $out) $($rest)+)
    }};
    (@step ($lvl:expr, $target:expr, $out:ident) $k:ident = $v:expr, $($rest:tt)+) => {{
        use ::std::fmt::Write as _;
        if !$out.is_empty() { $out.push(' '); }
        let _ = write!($out, "{}={}", stringify!($k), $v);
        $crate::__garage_log!(@step ($lvl, $target, $out) $($rest)+)
    }};
    (@step ($lvl:expr, $target:expr, $out:ident) $k:ident, $($rest:tt)+) => {{
        use ::std::fmt::Write as _;
        if !$out.is_empty() { $out.push(' '); }
        let _ = write!($out, "{}={}", stringify!($k), $k);
        $crate::__garage_log!(@step ($lvl, $target, $out) $($rest)+)
    }};
    (@step ($lvl:expr, $target:expr, $out:ident) $fmt:literal $($arg:tt)*) => {{
        $crate::logging::emit($lvl, $target, &$out, format_args!($fmt $($arg)*));
    }};
    ($lvl:expr, $($tt:tt)+) => {{
        let target = module_path!();
        if $crate::logging::is_enabled($lvl, target) {
            #[allow(unused_mut)]
            let mut fields = ::std::string::String::new();
            $crate::__garage_log!(@step ($lvl, target, fields) $($tt)+);
        }
    }};
}

pub mod tracing {
    #[macro_export]
    macro_rules! __garage_trace_error {
        ($($tt:tt)+) => { $crate::__garage_log!($crate::logging::Level::Error, $($tt)+) };
    }
    #[macro_export]
    macro_rules! __garage_trace_warn {
        ($($tt:tt)+) => { $crate::__garage_log!($crate::logging::Level::Warn, $($tt)+) };
    }
    #[macro_export]
    macro_rules! __garage_trace_info {
        ($($tt:tt)+) => { $crate::__garage_log!($crate::logging::Level::Info, $($tt)+) };
    }
    #[macro_export]
    macro_rules! __garage_trace_debug {
        ($($tt:tt)+) => { $crate::__garage_log!($crate::logging::Level::Debug, $($tt)+) };
    }

    pub use crate::__garage_trace_debug as debug;
    pub use crate::__garage_trace_error as error;
    pub use crate::__garage_trace_info as info;
    pub use crate::__garage_trace_warn as warn;
}
