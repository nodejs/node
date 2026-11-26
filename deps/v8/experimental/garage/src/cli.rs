// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Command-line parsing.
//!
//! Three invocation shapes:
//!
//! ```text
//! garage trace.log [more.log ...]     # files
//! d8 ... | garage                     # stdin is the data pipe
//! garage -- d8 --print-maglev-graphs x.js   # wrapper mode
//! ```
//!
//! The third one splits argv at `--` before flag parsing so the wrapped
//! command's own flags are never interpreted as garage's.

use std::ffi::OsString;
use std::fmt;
use std::path::PathBuf;
use std::process::ExitCode;

use anyhow::{Context, Result, bail};
use regex::Regex;

const HELP_TEXT: &str = "\
Terminal UI for viewing, navigating, searching and diffing d8 trace output.

Reads trace files, or stdin when d8 output is piped in. Keyboard input comes
from the controlling terminal in that case, so `d8 ... | garage` is fully
interactive.

Usage: garage [OPTIONS] [FILE]... [-- <COMMAND>...]

Arguments:
  [FILE]...  Trace files to open. Omit to read the trace from stdin

Options:
      --function <REGEX>  Only keep compilations whose function name matches this regex
      --config <PATH>     Config file to load instead of ~/.config/garage/config.toml
      --update            Replace this binary with the latest release and exit
      --debug             Write debug diagnostics to the log file
      --log-file <PATH>   Where diagnostics go when they are enabled [default: garage.log]
  -h, --help              Print help
  -V, --version           Print version
";

/// Early exit signal for `--help` / `--version` (exit code 0) vs usage errors.
#[derive(Debug)]
pub struct CliExit {
    pub message: String,
    pub code: ExitCode,
}

impl fmt::Display for CliExit {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for CliExit {}

#[derive(Debug)]
pub struct Cli {
    /// Trace files to open. Omit to read the trace from stdin.
    pub files: Vec<PathBuf>,
    /// Only keep compilations whose function name matches this regex.
    pub function: Option<String>,
    /// Config file to load instead of ~/.config/garage/config.toml.
    pub config: Option<PathBuf>,
    /// Replace this binary with the latest release and exit.
    pub update: bool,
    /// Write debug diagnostics to the log file.
    pub debug: bool,
    /// Where diagnostics go when they are enabled.
    pub log_file: PathBuf,
}

impl Cli {
    pub fn try_parse_from(args: Vec<OsString>) -> Result<Self> {
        let mut files = Vec::new();
        let mut function: Option<String> = None;
        let mut config: Option<PathBuf> = None;
        let mut update = false;
        let mut debug = false;
        let mut log_file = PathBuf::from("garage.log");

        let mut iter = args.into_iter();
        let _prog = iter.next(); // argv[0]

        while let Some(raw) = iter.next() {
            let s = raw.to_string_lossy();
            if s == "-h" || s == "--help" {
                return Err(CliExit {
                    message: HELP_TEXT.to_string(),
                    code: ExitCode::SUCCESS,
                }
                .into());
            }
            if s == "-V" || s == "--version" {
                return Err(CliExit {
                    message: format!("garage {}\n", env!("CARGO_PKG_VERSION")),
                    code: ExitCode::SUCCESS,
                }
                .into());
            }
            if s == "--update" {
                update = true;
            } else if s == "--debug" {
                debug = true;
            } else if s == "--function" {
                let val = iter.next().ok_or_else(|| {
                    anyhow::anyhow!("a value is required for '--function <REGEX>'")
                })?;
                function = Some(
                    val.into_string()
                        .map_err(|_| anyhow::anyhow!("--function regex must be valid UTF-8"))?,
                );
            } else if let Some(val) = s.strip_prefix("--function=") {
                function = Some(val.to_string());
            } else if s == "--config" {
                let val = iter
                    .next()
                    .ok_or_else(|| anyhow::anyhow!("a value is required for '--config <PATH>'"))?;
                config = Some(PathBuf::from(val));
            } else if let Some(val) = s.strip_prefix("--config=") {
                config = Some(PathBuf::from(val));
            } else if s == "--log-file" {
                let val = iter.next().ok_or_else(|| {
                    anyhow::anyhow!("a value is required for '--log-file <PATH>'")
                })?;
                log_file = PathBuf::from(val);
            } else if let Some(val) = s.strip_prefix("--log-file=") {
                log_file = PathBuf::from(val);
            } else if s.starts_with('-') && s != "-" {
                bail!("unexpected argument '{s}' found\n\nFor more information, try '--help'.");
            } else {
                files.push(PathBuf::from(raw));
            }
        }

        Ok(Self {
            files,
            function,
            config,
            update,
            debug,
            log_file,
        })
    }
}

/// A parsed command line, with the `--` tail separated out and the regex
/// compiled — both failures we would rather hit before touching the terminal.
#[derive(Debug)]
pub struct Invocation {
    pub cli: Cli,
    /// The command after `--`, if any. Non-empty when present.
    pub command: Option<Vec<OsString>>,
    pub function: Option<Regex>,
}

impl Invocation {
    pub fn from_env() -> Result<Self> {
        Self::from_argv(std::env::args_os())
    }

    pub fn from_argv<I, T>(argv: I) -> Result<Self>
    where
        I: IntoIterator<Item = T>,
        T: Into<OsString>,
    {
        let argv: Vec<OsString> = argv.into_iter().map(Into::into).collect();
        let (head, command) = split_at_double_dash(argv);

        let cli = Cli::try_parse_from(head)?;

        if let Some(cmd) = &command {
            if cmd.is_empty() {
                bail!(
                    "expected a command after `--`, e.g. `garage -- d8 --print-maglev-graphs x.js`"
                );
            }
            if !cli.files.is_empty() {
                bail!("cannot read files and wrap a command at the same time");
            }
        }

        // Compiled here so a bad pattern fails on the command line rather than
        // after the alternate screen is already up.
        let function = cli
            .function
            .as_deref()
            .map(Regex::new)
            .transpose()
            .context("--function is not a valid regex")?;

        Ok(Self {
            cli,
            command,
            function,
        })
    }
}

/// Splits argv at the first bare `--`.
///
/// Returns everything up to it (for flag parsing) and everything after it (the
/// wrapped command). `Some(vec![])` distinguishes a trailing `--` with nothing
/// after it from no `--` at all, so the caller can report it.
fn split_at_double_dash(argv: Vec<OsString>) -> (Vec<OsString>, Option<Vec<OsString>>) {
    match argv.iter().position(|a| a == "--") {
        Some(i) => {
            let mut head = argv;
            let tail = head.split_off(i);
            (head, Some(tail[1..].to_vec()))
        }
        None => (argv, None),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(args: &[&str]) -> Result<Invocation> {
        Invocation::from_argv(args.iter().map(OsString::from))
    }

    #[test]
    fn files_are_positional() {
        let inv = parse(&["garage", "a.log", "b.log"]).unwrap();
        assert_eq!(inv.cli.files.len(), 2);
        assert!(inv.command.is_none());
    }

    #[test]
    fn no_arguments_means_stdin() {
        let inv = parse(&["garage"]).unwrap();
        assert!(inv.cli.files.is_empty());
        assert!(inv.command.is_none());
    }

    #[test]
    fn double_dash_separates_the_wrapped_command() {
        let inv = parse(&[
            "garage",
            "--debug",
            "--",
            "d8",
            "--print-maglev-graphs",
            "x.js",
        ])
        .unwrap();
        assert!(inv.cli.debug);
        assert!(inv.cli.files.is_empty());
        assert_eq!(
            inv.command.unwrap(),
            vec!["d8", "--print-maglev-graphs", "x.js"]
        );
    }

    /// The whole reason for the manual split: without it the parser would
    /// swallow the wrapped command's own flags as garage's.
    #[test]
    fn wrapped_command_keeps_its_own_flags() {
        let inv = parse(&["garage", "--", "d8", "--debug", "--function", "foo"]).unwrap();
        assert!(!inv.cli.debug);
        assert!(inv.cli.function.is_none());
        assert_eq!(inv.command.unwrap().len(), 4);
    }

    #[test]
    fn trailing_double_dash_is_an_error() {
        assert!(parse(&["garage", "--"]).is_err());
    }

    #[test]
    fn files_and_wrapped_command_are_exclusive() {
        assert!(parse(&["garage", "a.log", "--", "d8", "x.js"]).is_err());
    }

    #[test]
    fn bad_function_regex_fails_at_parse_time() {
        assert!(parse(&["garage", "--function", "foo("]).is_err());
        assert!(parse(&["garage", "--function", "^process"]).is_ok());
    }
}
