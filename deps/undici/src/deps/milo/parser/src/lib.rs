#![allow(unused_imports)]

extern crate alloc;

use alloc::ffi::CString;
use alloc::vec::Vec;
use alloc::{boxed::Box, format};
use core::cell::{Cell, RefCell};
use core::ffi::{c_char, c_uchar, c_void};
use core::fmt::Debug;
use core::str;
use core::{mem, ptr};
use core::{slice, slice::from_raw_parts};

use milo_macros::generate;

#[repr(C)]
#[derive(Debug)]
pub struct Parser {
  // User writable
  #[cfg(not(target_family = "wasm"))]
  pub context: *mut c_void,
  pub max_start_line_length: usize,
  pub max_header_length: usize,
  pub max_body_payload: u64,
  pub autodetect: bool,
  pub is_request: bool,
  pub suspend_after_headers: bool,
  pub manage_unconsumed: bool,
  pub continue_without_data: bool,
  pub is_connect: bool,
  pub skip_body: bool,
  pub debug: bool,

  // Generic state
  pub parsed: u64,
  pub position: usize,
  pub state: u8,
  pub paused: bool,
  pub error_code: u8,

  // Current message flags
  pub content_length: u64,
  pub chunk_size: u64,
  pub remaining_content_length: u64,
  pub remaining_chunk_size: u64,
  pub status: u32,
  pub method: u8,
  pub has_content_length: bool,
  pub has_transfer_encoding: bool,
  pub has_chunked_transfer_encoding: bool,
  pub has_connection_close: bool,
  pub has_connection_upgrade: bool,
  pub has_upgrade: bool,
  pub has_trailers: bool,

  // Callback handling
  pub active_callbacks: u64,
  pub active_events: u64,
  #[cfg(not(target_family = "wasm"))]
  pub callbacks: ParserCallbacks,

  // WASM Specific
  #[cfg(target_family = "wasm")]
  pub ptr: *mut c_void,

  // Complex data types - We need to split them in order to be exportable to C++
  pub error_description: [u8; 255],
  pub unconsumed: *const c_uchar,
  pub unconsumed_len: usize,
  pub error_description_len: u8,

  // Event buffer. Keep this at the end of the struct for external readers.
  pub events: *mut c_uchar,
}

#[cfg(not(target_family = "wasm"))]
mod native;

#[cfg(not(target_family = "wasm"))]
pub use crate::native::*;

#[cfg(target_family = "wasm")]
mod wasm;

#[cfg(target_family = "wasm")]
pub use crate::wasm::*;

generate!();

impl Parser {
  /// Creates a new parser
  pub fn new() -> Parser {
    let mut events = Box::new([0u8; 65536]);
    let events_ptr = events.as_mut_ptr();
    mem::forget(events);

    Parser {
      // User writable
      #[cfg(not(target_family = "wasm"))]
      context: ptr::null_mut(),
      max_start_line_length: 8192,
      max_header_length: 8192,
      max_body_payload: 0,
      autodetect: true,
      is_request: false,
      suspend_after_headers: false,
      manage_unconsumed: false,
      continue_without_data: false,
      is_connect: false,
      skip_body: false,
      debug: false,
      // Generic state
      parsed: 0,
      position: 0,
      state: STATE_START,
      paused: false,
      error_code: ERROR_NONE,
      // Current message flags
      content_length: 0,
      chunk_size: 0,
      remaining_content_length: 0,
      remaining_chunk_size: 0,
      status: 0,
      method: 0,
      has_content_length: false,
      has_transfer_encoding: false,
      has_chunked_transfer_encoding: false,
      has_connection_close: false,
      has_connection_upgrade: false,
      has_upgrade: false,
      has_trailers: false,
      // Callbacks handling
      active_callbacks: 0,
      active_events: 0,
      #[cfg(not(target_family = "wasm"))]
      callbacks: ParserCallbacks::new(),
      // WASM Specific
      #[cfg(target_family = "wasm")]
      ptr: ptr::null_mut(),
      // Complex data types
      error_description: [0; 255],
      unconsumed: ptr::null(),
      unconsumed_len: 0,
      error_description_len: 0,
      events: events_ptr,
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
  ///   * manage_unconsumed
  ///   * continue_without_data
  ///   * debug
  ///   * context
  pub fn reset(&mut self, keep_parsed: bool) {
    self.state = STATE_START;
    self.paused = false;

    if !keep_parsed {
      self.parsed = 0;
    }

    self.error_code = ERROR_NONE;

    self.error_description[0] = 0;
    self.error_description_len = 0;

    self.clear_unconsumed();

    self.clear();
    self.skip_body = false;
    unsafe {
      *self.events = EVENT_END;
    }
  }

  pub(crate) fn clear_unconsumed(&mut self) {
    if self.unconsumed_len > 0 {
      // SAFETY: Retained input is allocated as a boxed slice with exactly this
      // length, and the parser owns it until replacement, reset, or destruction.
      unsafe {
        drop(Box::from_raw(ptr::slice_from_raw_parts_mut(
          self.unconsumed as *mut c_uchar,
          self.unconsumed_len,
        )));
      }
    }
    self.unconsumed = ptr::null();
    self.unconsumed_len = 0;
  }

  /// Clears all values about the message in the parser.
  pub fn clear(&mut self) {
    self.is_connect = false;
    self.method = 0;
    self.status = 0;
    self.has_content_length = false;
    self.has_transfer_encoding = false;
    self.has_chunked_transfer_encoding = false;
    self.has_connection_close = false;
    self.has_connection_upgrade = false;
    self.has_upgrade = false;
    self.has_trailers = false;
    self.content_length = 0;
    self.chunk_size = 0;
    self.remaining_content_length = 0;
    self.remaining_chunk_size = 0;
  }

  #[inline(always)]
  pub(crate) fn try_emit_event_range(
    &mut self,
    event_cursor: &mut usize,
    event_type: u8,
    at: usize,
    len: usize,
  ) -> bool {
    // Reserve six bytes for a later error event and one for the terminator.
    if *event_cursor + EVENT_RANGE_SIZE + EVENT_ERROR_RESERVE > EVENTS_BUFFER_SIZE {
      return false;
    }

    unsafe {
      *self.events.add(*event_cursor) = event_type;
      core::ptr::write_unaligned(self.events.add(*event_cursor + 1) as *mut u32, (at as u32).to_le());
      core::ptr::write_unaligned(self.events.add(*event_cursor + 5) as *mut u32, (len as u32).to_le());
    }
    *event_cursor += EVENT_RANGE_SIZE;
    true
  }

  #[inline(always)]
  pub(crate) fn try_emit_event_error(&mut self, event_cursor: &mut usize) -> bool {
    if *event_cursor + EVENT_ERROR_SIZE + EVENT_END_SIZE > EVENTS_BUFFER_SIZE {
      return false;
    }

    unsafe {
      *self.events.add(*event_cursor) = EVENT_ERROR;
      core::ptr::write_unaligned(
        self.events.add(*event_cursor + 1) as *mut u32,
        (self.position as u32).to_le(),
      );
      *self.events.add(*event_cursor + 5) = self.error_code;
    }
    *event_cursor += EVENT_ERROR_SIZE;
    true
  }

  /// Pauses the parser. It will have to be resumed via `resume`.
  pub fn pause(&mut self) { self.paused = true; }

  /// Resumes the parser.
  pub fn resume(&mut self) { self.paused = false; }

  /// Marks the parser as finished. Any new data received via `parse` will
  /// put the parser in the error state.
  pub fn finish(&mut self) {
    match self.state {
      // If the parser is one of the initial states, simply jump to finish
      STATE_START | STATE_REQUEST_LINE | STATE_STATUS_LINE | STATE_FINISH => {
        self.state = STATE_FINISH;
      }
      STATE_BODY_WITH_NO_LENGTH => {
        // Notify that the message has been completed
        let active_events = self.active_events | self.active_callbacks;
        let mut event_cursor = 0usize;
        if active_events & EVENT_ACTIVE_ON_MESSAGE_COMPLETE != 0 {
          self.try_emit_event_range(&mut event_cursor, EVENT_MESSAGE_COMPLETE, self.position, 0);
        }
        unsafe {
          *self.events.add(event_cursor) = EVENT_END;
        }

        // Set the state to be finished
        self.state = STATE_FINISH;
      }
      STATE_ERROR => (),
      // In another other state, this is an error
      _ => {
        self.fail(ERROR_UNEXPECTED_EOF, "Unexpected end of data");
      }
    }
  }

  /// Marks parsing as failed and starts a standalone error event batch.
  #[inline(always)]
  pub fn fail(&mut self, code: u8, description: &str) {
    self.set_error(code, description);
    let mut event_cursor = 0usize;
    if (self.active_events | self.active_callbacks) & EVENT_ACTIVE_ON_ERROR != 0 {
      self.try_emit_event_error(&mut event_cursor);
    }
    // SAFETY: The cursor follows at most one error event in the owned event buffer.
    unsafe {
      *self.events.add(event_cursor) = EVENT_END;
    }
  }

  /// Records an error without changing the current event batch.
  #[inline(always)]
  pub(crate) fn set_error(&mut self, code: u8, description: &str) {
    let bytes = description.as_bytes();
    let len = bytes.len().min(254);

    self.state = STATE_ERROR;
    self.error_code = code;
    self.error_description[..len].copy_from_slice(&bytes[..len]);
    self.error_description[len] = 0;
    self.error_description_len = len as u8;
  }

  /// Returns the current parser's state as string.
  pub fn state_str(&self) -> &str { States::try_from(self.state).unwrap().as_str() }

  /// Returns the current parser's error state as string.
  pub fn error_code_str(&self) -> &str { Errors::try_from(self.error_code).unwrap().as_str() }

  /// Returns the current parser's error description as string.
  pub fn error_description_str(&self) -> &str {
    unsafe {
      if self.error_description_len > 0 {
        str::from_utf8_unchecked(from_raw_parts(
          self.error_description.as_ptr(),
          self.error_description_len as usize,
        ))
      } else {
        ""
      }
    }
  }
}

impl Drop for Parser {
  fn drop(&mut self) {
    self.clear_unconsumed();
    if !self.events.is_null() {
      unsafe {
        let _ = Box::from_raw(self.events as *mut [u8; 65536]);
      }
      self.events = ptr::null_mut();
    }
  }
}

impl Default for Parser {
  fn default() -> Self { Self::new() }
}

mod matchers;
mod parse;
