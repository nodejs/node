#![allow(clippy::not_unsafe_ptr_arg_deref)]

use core::ptr;
use core::str;
use core::{slice, slice::from_raw_parts};
use std::ffi::{CString, c_char, c_uchar};

use crate::parse;
use crate::{Callbacks, Errors, Events, Methods, Parser, States};

#[repr(C)]
pub struct CStringWithLength {
  pub ptr: *const c_uchar,
  pub len: usize,
}

impl CStringWithLength {
  fn new(value: &str) -> CStringWithLength {
    let cstring = CString::new(value).unwrap();

    CStringWithLength {
      ptr: cstring.into_raw() as *const c_uchar,
      len: value.len(),
    }
  }
}

impl From<&str> for CStringWithLength {
  fn from(value: &str) -> Self { CStringWithLength::new(value) }
}

impl From<CStringWithLength> for &str {
  fn from(value: CStringWithLength) -> Self {
    unsafe { str::from_utf8_unchecked(slice::from_raw_parts(value.ptr, value.len)) }
  }
}

/// Returns if debug informations are available in this build.
#[unsafe(no_mangle)]
pub extern "C" fn milo_has_debug() -> bool { cfg!(any(debug_assertions, feature = "debug")) }

/// Returns if debug tracing is enabled for this parser.
#[unsafe(no_mangle)]
pub extern "C" fn milo_is_debug(parser: *const Parser) -> bool { unsafe { (*parser).debug } }

/// A callback that simply returns `0`.
///
/// Use this callback as pointer when you want to remove a callback from the
/// parser.
#[unsafe(no_mangle)]
pub extern "C" fn milo_noop(_parser: &mut Parser, _at: usize, _len: usize) {}

/// Cleans up memory used by a string previously returned by one of the milo's C
/// public interface.
#[unsafe(no_mangle)]
pub extern "C" fn milo_free_string(s: CStringWithLength) {
  unsafe {
    let _ = CString::from_raw(s.ptr as *mut c_char);
  }
}

/// Creates a new parser.
#[unsafe(no_mangle)]
pub extern "C" fn milo_create() -> *mut Parser { Box::into_raw(Box::new(Parser::new())) }

/// Destroys a parser.
#[unsafe(no_mangle)]
pub extern "C" fn milo_destroy(parser: *mut Parser) {
  if parser.is_null() {
    return;
  }

  unsafe {
    let _ = Box::from_raw(parser);
  }
}

/// Parses a slice of characters. It returns the number of consumed characters.
#[unsafe(no_mangle)]
pub extern "C" fn milo_parse(parser: *mut Parser, data: *const c_uchar, limit: usize) -> usize {
  unsafe { (*parser).parse(data, limit) }
}

/// Sets the parser event bitmask.
#[unsafe(no_mangle)]
pub extern "C" fn milo_set_active_events(parser: *mut Parser, value: u64) {
  unsafe {
    (*parser).active_events = value;
  }
}

/// Sets the maximum body payload consumed by a single parse invocation.
#[unsafe(no_mangle)]
pub extern "C" fn milo_set_max_body_payload(parser: *mut Parser, value: u64) {
  unsafe {
    (*parser).max_body_payload = value;
  }
}

/// Sets whether parsing should stop after headers have completed.
#[unsafe(no_mangle)]
pub extern "C" fn milo_set_suspend_after_headers(parser: *mut Parser, value: bool) {
  unsafe {
    (*parser).suspend_after_headers = value;
  }
}

/// Resets a parser. The second parameters specifies if to also reset the
/// parsed counter.
///
/// The following fields are not modified:
///   * position
///   * context
///   * autodetect
///   * is_request
///   * suspend_after_headers
///   * max_body_payload
///   * manage_unconsumed
///   * continue_without_data
///   * debug
///   * context
#[unsafe(no_mangle)]
pub extern "C" fn milo_reset(parser: *mut Parser, keep_parsed: bool) { unsafe { (*parser).reset(keep_parsed) } }

/// Clears all values about the message in the parser.
///
/// The autodetect and is_request fields are not cleared.
#[unsafe(no_mangle)]
pub extern "C" fn milo_clear(parser: *mut Parser) { unsafe { (*parser).clear() } }

/// Pauses the parser. It will have to be resumed via `milo_resume`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_pause(parser: *mut Parser) { unsafe { (*parser).pause() } }

/// Resumes the parser.
#[unsafe(no_mangle)]
pub extern "C" fn milo_resume(parser: *mut Parser) { unsafe { (*parser).resume() } }

/// Completes the current message without consuming more input.
#[unsafe(no_mangle)]
pub extern "C" fn milo_complete(parser: *mut Parser) { unsafe { (*parser).complete() } }

/// Marks the parser as finished. Any new data received via `milo_parse` will
/// put the parser in the error state.
#[unsafe(no_mangle)]
pub extern "C" fn milo_finish(parser: *mut Parser) { unsafe { (*parser).finish() } }

/// Marks the parsing a failed, setting a error code and and error message.
#[unsafe(no_mangle)]
pub extern "C" fn milo_fail(parser: *mut Parser, code: u8, description: CStringWithLength) {
  unsafe { (*parser).fail(code, description.into()) };
}

/// Returns the current parser's state as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_state_string(parser: *mut Parser) -> CStringWithLength {
  unsafe { (*parser).state_str().into() }
}

/// Returns a parser method as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_method_to_string(method: u8) -> CStringWithLength {
  Methods::try_from(method)
    .map_or("UNKNOWN", |method| method.as_str())
    .into()
}

/// Returns a parser error as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_error_to_string(error: u8) -> CStringWithLength {
  Errors::try_from(error).map_or("UNKNOWN", |error| error.as_str()).into()
}

/// Returns a parser callback as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_callback_to_string(callback: u8) -> CStringWithLength {
  Callbacks::try_from(callback)
    .map_or("UNKNOWN", |callback| callback.as_str())
    .into()
}

/// Returns a parser state as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_state_to_string(state: u8) -> CStringWithLength {
  States::try_from(state).map_or("UNKNOWN", |state| state.as_str()).into()
}

/// Returns a parser event as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_event_to_string(event: u8) -> CStringWithLength {
  Events::try_from(event).map_or("UNKNOWN", |event| event.as_str()).into()
}

/// Returns the current parser's error state as string.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_error_code_string(parser: *mut Parser) -> CStringWithLength {
  unsafe { (*parser).error_code_str().into() }
}

/// Returns the current parser's error descrition.
///
/// The returned value must be freed using `free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn milo_error_description_string(parser: *mut Parser) -> CStringWithLength {
  unsafe { (*parser).error_description_str().into() }
}
