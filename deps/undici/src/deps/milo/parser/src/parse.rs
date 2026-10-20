#![allow(clippy::not_unsafe_ptr_arg_deref)]

extern crate alloc;

use alloc::vec::Vec;
use alloc::{boxed::Box, format};
use core::cell::{Cell, RefCell};
use core::ffi::{c_char, c_uchar, c_void};
use core::fmt::Debug;
use core::ptr;
use core::str;
use core::{slice, slice::from_raw_parts};
use std::string;
#[cfg(all(not(target_family = "wasm"), any(debug_assertions, feature = "debug")))]
use std::time::Instant;

use milo_macros::*;

use crate::Methods::CONNECT;
use crate::matchers::*;
use crate::*;

impl Parser {
  /// Parses a slice of characters.
  ///
  /// It returns the number of consumed characters.
  pub fn parse(&mut self, input: *const c_uchar, limit: usize) -> usize {
    let mut event_cursor = 0usize;
    let mut failed = false;
    let active_events = self.active_events | self.active_callbacks;

    // Error states cannot resume without reset. Do not aggregate or retain new
    // input on retries, which would otherwise grow the pending buffer forever.
    if self.state == STATE_ERROR {
      self.position = 0;
    }

    // Paused and failed parsers consume nothing and produce an empty event batch.
    if self.paused || self.state == STATE_ERROR {
      if active_events != 0 {
        // SAFETY: The live parser owns the event buffer and its first byte.
        unsafe {
          *self.events = EVENT_END;
        }
      }

      return 0;
    }

    let input = unsafe { from_raw_parts(input, limit) };

    // Set the data to analyze, prepending unconsumed data from previous iteration
    // if needed
    let mut limit = limit;
    let aggregate: Vec<c_uchar>;
    let unconsumed_len = self.unconsumed_len;

    let mut data = if self.manage_unconsumed && unconsumed_len > 0 {
      unsafe {
        limit += unconsumed_len;
        let unconsumed = from_raw_parts(self.unconsumed, unconsumed_len);

        // Rebuild a contiguous view so state handlers can scan across parse-call
        // boundaries.
        aggregate = [unconsumed, input].concat();
        &aggregate[..]
      }
    } else {
      input
    };

    // Limit the data that is currently analyzed
    data = &data[..limit];

    let mut available = data.len();

    #[cfg(all(not(target_family = "wasm"), any(debug_assertions, feature = "debug")))]
    let mut last = Instant::now();

    #[cfg(all(not(target_family = "wasm"), any(debug_assertions, feature = "debug")))]
    let start = Instant::now();

    #[cfg(any(debug_assertions, feature = "debug"))]
    let mut previous_state = self.state;

    #[cfg(any(debug_assertions, feature = "debug"))]
    let previous_position = self.position;

    // States will advance position manually, the parser has to explicitly
    // track it
    self.position = 0;
    let mut advanced: usize;
    let mut parsing = true;
    let max_body_payload = self.max_body_payload;
    let mut body_payload_read = 0u64;
    let has_active_events = active_events != 0;
    let (
      has_request_start_events,
      has_response_start_events,
      has_request_line_events,
      has_response_line_events,
      has_metadata_event,
      has_header_name_event,
      has_header_value_event,
      has_trailer_name_event,
      has_trailer_value_event,
      has_complete_events,
      has_finish_event,
    ) = if has_active_events {
      (
        active_events & (EVENT_ACTIVE_ON_REQUEST | EVENT_ACTIVE_ON_MESSAGE_START) != 0,
        active_events & (EVENT_ACTIVE_ON_RESPONSE | EVENT_ACTIVE_ON_MESSAGE_START) != 0,
        active_events
          & (EVENT_ACTIVE_ON_METHOD | EVENT_ACTIVE_ON_URL | EVENT_ACTIVE_ON_PROTOCOL | EVENT_ACTIVE_ON_VERSION)
          != 0,
        active_events
          & (EVENT_ACTIVE_ON_PROTOCOL | EVENT_ACTIVE_ON_VERSION | EVENT_ACTIVE_ON_STATUS | EVENT_ACTIVE_ON_REASON)
          != 0,
        active_events & EVENT_ACTIVE_ON_HEADERS != 0,
        active_events & EVENT_ACTIVE_ON_HEADER_NAME != 0,
        active_events & EVENT_ACTIVE_ON_HEADER_VALUE != 0,
        active_events & EVENT_ACTIVE_ON_TRAILER_NAME != 0,
        active_events & EVENT_ACTIVE_ON_TRAILER_VALUE != 0,
        active_events & (EVENT_ACTIVE_ON_MESSAGE_COMPLETE | EVENT_ACTIVE_ON_RESET) != 0,
        active_events & EVENT_ACTIVE_ON_FINISH != 0,
      )
    } else {
      (
        false, false, false, false, false, false, false, false, false, false, false,
      )
    };

    #[cfg(any(debug_assertions, feature = "debug"))]
    if self.debug {
      eprintln!("[milo_parser::debug] loop enter");
    }

    // Until there is data or there is a request to continue
    'parser: while parsing && (!self.paused) && (available != 0 || self.continue_without_data) {
      #[cfg(any(debug_assertions, feature = "debug"))]
      if self.debug {
        eprintln!(
          "[milo_parser::debug] loop before processing: previous_position={}, position={}, available={}, \
           continue_without_data={}",
          previous_position, self.position, available, self.continue_without_data
        );
      }

      // Reset the flag before processing; states set it again when they need another
      // zero-byte turn.
      self.continue_without_data = false;
      advanced = 0;

      'state: {
        match self.state {
          // If the parser has finished and it receives more data, error
          STATE_FINISH => {
            fail!(UNEXPECTED_CHARACTER, "Unexpected data");
          }

          // The error state is a no-op
          STATE_ERROR => {
            suspend!();
          }

          // Choose the initial state depending on the configured message type.
          STATE_START => {
            if !self.autodetect && self.is_request {
              if has_request_start_events {
                event_with_range!(on_request, 0, 0);
                event_with_range!(on_message_start, 0, 0);
              }
              move_to!(request_line);
            } else if !self.autodetect {
              if has_response_start_events {
                event_with_range!(on_response, 0, 0);
                event_with_range!(on_message_start, 0, 0);
              }
              move_to!(status_line);
            } else if data.len() >= 5 && data[4] == b'/' && data.starts_with(b"HTTP") {
              self.is_request = false;
              if has_response_start_events {
                event_with_range!(on_response, 0, 0);
                event_with_range!(on_message_start, 0, 0);
              }
              move_to!(status_line);
            } else if data.len() >= 2 && data.starts_with(b"\r\n") {
              // RFC 9112 section 2.2
              advance!(2);
            } else {
              // For performance reason, we assume it's a request so we don't lookup the
              // method twice
              self.is_request = true;
              if has_request_start_events {
                event_with_range!(on_request, 0, 0);
                event_with_range!(on_message_start, 0, 0);
              }
              move_to!(request_line);
            }
          }

          STATE_REQUEST_LINE => {
            match find_cr(data, available) {
              // // RFC 9112 section 3
              Some(cr) => {
                match ensure_valid_line(data, cr, available) {
                  MatchResult::Continue => {}
                  MatchResult::Suspend => {
                    suspend!();
                  }
                  MatchResult::Stop => {
                    fail!(UNEXPECTED_CHARACTER, "Expected CRLF");
                  }
                }

                // RFC 9112 section 2.2 - Repeated
                if cr == 0 {
                  advance!(2);
                  next!();
                } else if cr < 14
                // Length of "GET / HTTP/1.1"
                {
                  fail!(UNEXPECTED_CHARACTER, "Request line too short");
                }

                // The line is potentially valid, clear the parser
                self.clear();

                // RFC 9112 section 3.1
                let method_start = 0;
                let method_end = match find_char(data, method_start, cr, b' ') {
                  Some(index) if index > method_start => index,
                  _ => {
                    fail!(UNEXPECTED_CHARACTER, "Expected space after method");
                  }
                };

                // RFC 9112 section 3.2
                let url_start = method_end + 1;
                let url_end = match find_char(data, url_start, cr, b' ') {
                  Some(index) if index > url_start => index,
                  _ => {
                    fail!(UNEXPECTED_CHARACTER, "Expected space after URL");
                  }
                };

                // RFC 9112 section 2.3
                let protocol_start = url_end + 1;
                let protocol_end = match find_char(data, protocol_start, cr, b'/') {
                  Some(index) if index > protocol_start => index,
                  _ => {
                    fail!(UNEXPECTED_CHARACTER, "Expected / after the protocol name");
                  }
                };

                let method_slice = &data[method_start..method_end];
                self.method = match method_slice.len() {
                  3 => {
                    match method_slice {
                      b"GET" => METHOD_GET,
                      b"PUT" => METHOD_PUT,
                      b"PRI" => METHOD_PRI,
                      _ => METHOD_OTHER,
                    }
                  }
                  4 => {
                    match method_slice {
                      b"HEAD" => METHOD_HEAD,
                      b"POST" => METHOD_POST,
                      _ => METHOD_OTHER,
                    }
                  }
                  5 => {
                    match method_slice {
                      b"PATCH" => METHOD_PATCH,
                      b"TRACE" => METHOD_TRACE,
                      _ => METHOD_OTHER,
                    }
                  }
                  6 => {
                    match method_slice {
                      b"DELETE" => METHOD_DELETE,
                      _ => METHOD_OTHER,
                    }
                  }
                  7 => {
                    match method_slice {
                      b"CONNECT" => {
                        self.is_connect = true;
                        METHOD_CONNECT
                      }
                      b"OPTIONS" => METHOD_OPTIONS,
                      _ => METHOD_OTHER,
                    }
                  }
                  _ => METHOD_OTHER,
                };

                if self.method == METHOD_OTHER && !validate_token(data, method_start, method_end) {
                  fail!(UNEXPECTED_CHARACTER, "Invalid method character");
                }

                if !validate_url(data, url_start, url_end) {
                  fail!(UNEXPECTED_CHARACTER, "Invalid URL character");
                }

                let version_start = protocol_end + 1;
                if cr != protocol_start + 8 {
                  fail!(UNEXPECTED_CHARACTER, "Invalid protocol name");
                }

                if &data[protocol_start..cr] == b"HTTP/1.1" {
                  if self.method == METHOD_PRI {
                    fail!(UNSUPPORTED_HTTP_VERSION, "PRI is only valid with HTTP/2.0");
                  }
                } else if &data[protocol_start..cr] == b"HTTP/2.0" {
                  if self.method != METHOD_PRI {
                    fail!(UNSUPPORTED_HTTP_VERSION, "Unsupported HTTP version");
                  }
                } else {
                  fail!(UNEXPECTED_CHARACTER, "Invalid protocol");
                }

                if has_request_line_events {
                  event_with_range!(on_method, method_start, method_end - method_start);
                  event_with_range!(on_url, url_start, url_end - url_start);
                  event_with_range!(on_protocol, protocol_start, protocol_end - protocol_start);
                  event_with_range!(on_version, version_start, 3);
                }

                advance!(cr + 2);

                if self.method == METHOD_PRI {
                  move_to!(http2_preface);
                } else {
                  move_to!(header);
                }
              }
              None => {
                if available >= self.max_start_line_length {
                  fail!(UNEXPECTED_CHARACTER, "Request line too long");
                } else {
                  suspend!();
                }
              }
            }
          }

          // RFC 9112 section 4
          STATE_STATUS_LINE => {
            match find_cr(data, available) {
              Some(cr) => {
                match ensure_valid_line(data, cr, available) {
                  MatchResult::Continue => {}
                  MatchResult::Suspend => {
                    suspend!();
                  }
                  MatchResult::Stop => {
                    fail!(UNEXPECTED_CHARACTER, "Expected CRLF");
                  }
                }

                // RFC 9112 section 2.2 - Repeated
                if cr == 0 {
                  advance!(2);
                  next!();
                } else if cr < 13
                // Length of "HTTP/1.1 200 "
                {
                  fail!(UNEXPECTED_CHARACTER, "Status line too short");
                }

                // The line is potentially valid, clear the parser
                self.clear();

                let protocol_start = 0;
                let protocol_end = 4;
                let version_start = protocol_end + 1;
                let version_end = protocol_start + 8;

                if cr < version_end || data[version_end] != b' ' {
                  fail!(UNEXPECTED_CHARACTER, "Expected space after protocol");
                }

                match &data[protocol_start..version_end] {
                  b"HTTP/1.1" => {}
                  [b'H', b'T', b'T', b'P', b'/', ..] => {
                    fail!(UNSUPPORTED_HTTP_VERSION, "Unsupported HTTP version");
                  }
                  _ => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid protocol");
                  }
                }

                let status_start = version_end + 1;
                // Even if the reason is empty, there must be at least a space after the status
                // code. So three digits plus a space
                let status_end = version_end + 5;
                if status_end > cr {
                  fail!(INVALID_STATUS, "Expected HTTP response status");
                }

                if !is_digit(data[status_start])
                  || !is_digit(data[status_start + 1])
                  || !is_digit(data[status_start + 2])
                {
                  fail!(INVALID_STATUS, "Invalid HTTP response status");
                }

                if data[status_start + 3] != b' ' {
                  fail!(INVALID_STATUS, "Expected a space after HTTP response status");
                }

                let reason_start = status_start + 4;
                let reason_end = cr;
                if reason_start != reason_end
                  && unsafe { !validate_token_value(data.as_ptr().add(reason_start), reason_end - reason_start) }
                {
                  fail!(UNEXPECTED_CHARACTER, "Invalid status reason character");
                }

                self.status = ((data[status_start] - b'0') as u32) * 100
                  + ((data[status_start + 1] - b'0') as u32) * 10
                  + (data[status_start + 2] - b'0') as u32;

                if has_response_line_events {
                  event_with_range!(on_protocol, protocol_start, 4);
                  event_with_range!(on_version, version_start, 3);
                  event_with_range!(on_status, status_start, 3);
                  if reason_end > reason_start {
                    event_with_range!(on_reason, reason_start, reason_end - reason_start);
                  }
                }

                advance!(cr + 2);
                move_to!(header);
              }
              None => {
                if available >= self.max_start_line_length {
                  fail!(UNEXPECTED_CHARACTER, "Status line too long");
                } else {
                  suspend!();
                }
              }
            }
          }

          STATE_HTTP2_PREFACE => {
            if available < 8 {
              suspend!();
            } else if &data[..8] == b"\r\nSM\r\n\r\n" {
              advance!(8);
              move_to!(tunnel);
            } else {
              fail!(UNEXPECTED_CHARACTER, "Malformed HTTP/2.0 preface");
            }
          }

          STATE_HEADER => {
            match find_header_line_end(data.as_ptr(), available) {
              HeaderLineScanResult::Cr(cr) => {
                match ensure_valid_line(data, cr, available) {
                  MatchResult::Continue => {}
                  MatchResult::Suspend => {
                    suspend!();
                  }
                  MatchResult::Stop => {
                    fail!(UNEXPECTED_CHARACTER, "Expected CRLF");
                  }
                }

                // No more headers or no headers at all, move to the headers state
                if cr == 0 {
                  advance!(2);
                  if has_metadata_event {
                    event_with_metadata!(on_headers, 2);
                  }
                  move_to!(body_decision);
                  self.continue_without_data = true;

                  if self.suspend_after_headers {
                    suspend!();
                  } else {
                    next!();
                  }
                }

                // RFC 9112 section.4
                // RFC 9110 section 5.5 and 5.6
                let header_name_start = 0;
                let header_name_end = match find_char(data, header_name_start, cr, b':') {
                  Some(index) if index > header_name_start => index,
                  _ => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid header field name character");
                  }
                };
                let mut header_value_start = header_name_end + 1;
                let mut header_value_end = cr;

                let status = self.status;
                let first_header_byte = data[header_name_start];
                if !matches!(first_header_byte, b'c' | b'C' | b't' | b'T' | b'u' | b'U') {
                  if !validate_token(data, header_name_start, header_name_end) {
                    fail!(UNEXPECTED_CHARACTER, "Invalid header field name character");
                  }

                  if has_header_value_event {
                    strip_ows_fast(data, &mut header_value_start, &mut header_value_end, true);
                  }
                } else {
                  let header_name_len = header_name_end - header_name_start;
                  match (header_name_len, &data[header_name_start..header_name_end]) {
                    // RFC 9112 section 6.2
                    (14, case_insensitive_string!("content-length")) => {
                      if self.has_transfer_encoding {
                        fail!(
                          UNEXPECTED_CONTENT_LENGTH,
                          "Unexpected Content-Length header when Transfer-Encoding header is present"
                        );
                      } else if status == 205 || status == 204 || status / 100 == 1 {
                        fail!(
                          UNEXPECTED_CONTENT_LENGTH,
                          "Unexpected Content-Length header for a response without body"
                        );
                      } else if self.has_content_length {
                        fail!(INVALID_CONTENT_LENGTH, "Invalid duplicate Content-Length header");
                      }

                      if header_value_start < cr && !is_ws(data[cr - 1]) {
                        let value_start = if data[header_value_start] == b' ' {
                          header_value_start + 1
                        } else {
                          header_value_start
                        };

                        if value_start < cr && !is_ws(data[value_start]) {
                          header_value_start = value_start;
                        } else if !strip_ows_fast(data, &mut header_value_start, &mut header_value_end, false) {
                          fail!(UNEXPECTED_CHARACTER, "Expected Content-Length header value");
                        }
                      } else if !strip_ows_fast(data, &mut header_value_start, &mut header_value_end, false) {
                        fail!(UNEXPECTED_CHARACTER, "Expected Content-Length header value");
                      }

                      let mut i = header_value_start;
                      let mut content_length = 0u64;

                      if header_value_end - header_value_start > 19 {
                        // Milo caps Content-Length at 19 digits as a practical limit. This keeps
                        // parsing overflow-safe while allowing values far
                        // beyond realistic message sizes.
                        fail!(INVALID_CONTENT_LENGTH, "Invalid Content-Length header");
                      }

                      while i < header_value_end {
                        let current = data[i];
                        if !is_digit(current) {
                          fail!(INVALID_CONTENT_LENGTH, "Invalid Content-Length header");
                        }

                        content_length = content_length * 10 + (current - b'0') as u64;
                        i += 1;
                      }

                      self.has_content_length = true;
                      self.content_length = content_length;
                      self.remaining_content_length = content_length;
                    }
                    // RFC 9112 section 6.1
                    (17, case_insensitive_string!("transfer-encoding")) => {
                      if self.has_content_length {
                        fail!(
                          UNEXPECTED_TRANSFER_ENCODING,
                          "Unexpected Transfer-Encoding header when Content-Length header is present"
                        );
                      } else if status == 304 || status == 205 || status == 204 || status / 100 == 1 {
                        fail!(
                          UNEXPECTED_TRANSFER_ENCODING,
                          "Unexpected Transfer-Encoding header for a response without body"
                        );
                      }

                      if !strip_ows_fast(data, &mut header_value_start, &mut header_value_end, false) {
                        fail!(UNEXPECTED_CHARACTER, "Expected Transfer-Encoding header value");
                      }

                      self.has_transfer_encoding = true;

                      if &data[header_value_start..header_value_end] == b"chunked" {
                        // If this is true, it means the Transfer-Encoding header was specified more
                        // than once. This is the second repetition and therefore, the previous one is
                        // no longer the last one, making it invalid.
                        if self.has_chunked_transfer_encoding {
                          fail!(
                            INVALID_TRANSFER_ENCODING,
                            "The value \"chunked\" in the Transfer-Encoding header must be the last provided and can \
                             be provided only once"
                          );
                        }

                        self.has_chunked_transfer_encoding = true;
                      } else {
                        let mut token_start = header_value_start;
                        loop {
                          while token_start < header_value_end && is_ws(data[token_start]) {
                            token_start += 1;
                          }

                          if token_start == header_value_end {
                            break;
                          }

                          let token_end_raw = match find_char(data, token_start, header_value_end, b',') {
                            Some(comma) => comma,
                            None => header_value_end,
                          };
                          let mut token_end = token_end_raw;

                          if !strip_ows_fast(data, &mut token_start, &mut token_end, false) {
                            fail!(UNEXPECTED_CHARACTER, "Expected Transfer-Encoding header value");
                          }

                          self.has_transfer_encoding = true;

                          if let case_insensitive_string!("chunked") = data[token_start..token_end] {
                            // If this is true, it means the Transfer-Encoding header was specified more
                            // than once. This is the second repetition and therefore, the previous one is
                            // no longer the last one, making it invalid.
                            if self.has_chunked_transfer_encoding {
                              fail!(
                                INVALID_TRANSFER_ENCODING,
                                "The value \"chunked\" in the Transfer-Encoding header must be the last provided and \
                                 can be provided only once"
                              );
                            }

                            self.has_chunked_transfer_encoding = true;
                          } else {
                            if self.has_chunked_transfer_encoding {
                              // Any other value when chunked was already specified is invalid as the previous
                              // chunked would not be the last one anymore
                              fail!(
                                INVALID_TRANSFER_ENCODING,
                                "The value \"chunked\" in the Transfer-Encoding header must be the last provided"
                              );
                            }
                          }

                          if token_end_raw == header_value_end {
                            break;
                          } else {
                            token_start = token_end_raw + 1;
                          }
                        }
                      }
                    }
                    // RFC 9112 section 9.6
                    (10, case_insensitive_string!("connection")) => {
                      if !strip_ows_fast(data, &mut header_value_start, &mut header_value_end, false) {
                        fail!(UNEXPECTED_CHARACTER, "Expected Connection header value");
                      }

                      match data[header_value_start..header_value_end] {
                        case_insensitive_string!("close") => {
                          self.has_connection_close = true;
                        }
                        case_insensitive_string!("keep-alive") => {
                          // Keep-alive is implicit unless Connection: close is
                          // present.
                        }
                        case_insensitive_string!("upgrade") => {
                          self.has_connection_upgrade = true;
                        }
                        _ => {
                          // Comma separated values
                          let mut token_start = header_value_start;
                          loop {
                            while token_start < header_value_end && is_ws(data[token_start]) {
                              token_start += 1;
                            }

                            if token_start == header_value_end {
                              break;
                            }

                            let token_end_raw = match find_char(data, token_start, header_value_end, b',') {
                              Some(comma) => comma,
                              None => header_value_end,
                            };
                            let mut token_end = token_end_raw;

                            if !strip_ows_fast(data, &mut token_start, &mut token_end, false) {
                              fail!(UNEXPECTED_CHARACTER, "Expected Connection header value");
                            }

                            match data[token_start..token_end] {
                              case_insensitive_string!("close") => {
                                self.has_connection_close = true;
                              }
                              case_insensitive_string!("upgrade") => {
                                self.has_connection_upgrade = true;
                              }
                              case_insensitive_string!("keep-alive") => {}
                              _ => {
                                if !validate_token(data, token_start, token_end) {
                                  fail!(UNEXPECTED_CHARACTER, "Invalid Connection header value");
                                }
                              }
                            }

                            if token_end_raw == header_value_end {
                              break;
                            } else {
                              token_start = token_end_raw + 1;
                            }
                          }
                        }
                      }
                    }
                    (7, case_insensitive_string!("trailer")) => {
                      self.has_trailers = true;

                      if !strip_ows_fast(data, &mut header_value_start, &mut header_value_end, false) {
                        fail!(UNEXPECTED_CHARACTER, "Expected Trailer header value");
                      }
                    }
                    (7, case_insensitive_string!("upgrade")) => {
                      if !strip_ows_fast(data, &mut header_value_start, &mut header_value_end, false) {
                        fail!(UNEXPECTED_CHARACTER, "Expected Upgrade header value");
                      }

                      let mut token_start = header_value_start;
                      loop {
                        while token_start < header_value_end && is_ws(data[token_start]) {
                          token_start += 1;
                        }

                        if token_start == header_value_end {
                          break;
                        }

                        let token_end_raw = match find_char(data, token_start, header_value_end, b',') {
                          Some(comma) => comma,
                          None => header_value_end,
                        };
                        let mut token_end = token_end_raw;

                        if !strip_ows_fast(data, &mut token_start, &mut token_end, false) {
                          fail!(UNEXPECTED_CHARACTER, "Expected Upgrade header value");
                        }

                        let protocol_name_end = find_char(data, token_start, token_end, b'/').unwrap_or(token_end);
                        if !validate_token(data, token_start, protocol_name_end) {
                          fail!(UNEXPECTED_CHARACTER, "Invalid Upgrade header value");
                        }

                        if protocol_name_end < token_end {
                          let protocol_version_start = protocol_name_end + 1;
                          if find_char(data, protocol_version_start, token_end, b'/').is_some()
                            || !validate_token(data, protocol_version_start, token_end)
                          {
                            fail!(UNEXPECTED_CHARACTER, "Invalid Upgrade header value");
                          }
                        }

                        if token_end_raw == header_value_end {
                          break;
                        } else {
                          token_start = token_end_raw + 1;
                        }
                      }

                      self.has_upgrade = true;
                    }
                    _ => {
                      if !validate_token(data, header_name_start, header_name_end) {
                        fail!(UNEXPECTED_CHARACTER, "Invalid header field name character");
                      }

                      if has_header_value_event {
                        strip_ows_fast(data, &mut header_value_start, &mut header_value_end, true);
                      }
                    }
                  }
                }

                if has_header_name_event {
                  event_with_range!(on_header_name, header_name_start, header_name_end - header_name_start);
                }

                if has_header_value_event {
                  event_with_range!(
                    on_header_value,
                    header_value_start,
                    header_value_end - header_value_start
                  );
                }

                advance!(cr + 2);
              }
              HeaderLineScanResult::Invalid(invalid) => {
                match find_char(data, 0, invalid, b':') {
                  Some(_) => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid header field value character");
                  }
                  None => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid header field name character");
                  }
                }
              }
              HeaderLineScanResult::Incomplete => {
                if available >= self.max_header_length {
                  fail!(UNEXPECTED_CHARACTER, "Header line too long");
                } else {
                  suspend!();
                }
              }
            }
          }

          // RFC 9110 section 9.3.6 and 7.8 - Headers have finished, check if the
          // connection must be upgraded or a body is expected.
          STATE_BODY_DECISION => {
            let method = self.method;
            let status = self.status;

            if self.has_upgrade && !self.has_connection_upgrade {
              fail!(
                MISSING_CONNECTION_UPGRADE,
                "Missing Connection header set to \"upgrade\" when using the Upgrade header"
              );
            }

            if self.has_trailers && !self.has_chunked_transfer_encoding {
              fail!(
                UNEXPECTED_TRAILERS,
                "Trailers are not allowed when not using chunked transfer encoding"
              );
            } else if self.is_request && (method == METHOD_GET || method == METHOD_HEAD) && self.content_length > 0 {
              fail!(UNEXPECTED_CONTENT, "Unexpected content for the request (GET or HEAD)");
            }

            // In case of Connection: Upgrade or a CONNECT method
            if self.is_connect {
              // In case of CONNECT method
              event_with_range!(on_connect, 0, 0);
              move_to!(tunnel);
            } else if self.has_upgrade && !self.is_request && status == 101 {
              event_with_range!(on_upgrade, 0, 0);
              move_to!(tunnel);
            } else if self.is_request {
              if self.has_transfer_encoding && !self.has_chunked_transfer_encoding {
                fail!(
                  UNEXPECTED_CONTENT_LENGTH,
                  "Transfer-Encoding last header value must be \"chunked\" if the header is present"
                );
              } else if self.skip_body {
                self.continue_without_data = true;
                if !self.complete_message(
                  0,
                  active_events,
                  has_complete_events,
                  has_finish_event,
                  &mut event_cursor,
                ) {
                  suspend!();
                }
              } else if self.has_content_length {
                // RFC 9110 section 6.3
                if self.content_length == 0 {
                  self.continue_without_data = true;
                  if !self.complete_message(
                    0,
                    active_events,
                    has_complete_events,
                    has_finish_event,
                    &mut event_cursor,
                  ) {
                    suspend!();
                  }
                } else {
                  move_to!(body_via_content_length);
                }
              } else if !self.has_chunked_transfer_encoding {
                self.continue_without_data = true;
                if !self.complete_message(
                  0,
                  active_events,
                  has_complete_events,
                  has_finish_event,
                  &mut event_cursor,
                ) {
                  suspend!();
                }
              } else {
                move_to!(chunk_header);
              }
            } else {
              // Response
              // RFC 9110 section 15.4.5
              if self.skip_body || (status < 200 && status != 101) || status == 204 || status == 205 || status == 304 {
                self.continue_without_data = true;
                if !self.complete_message(
                  0,
                  active_events,
                  has_complete_events,
                  has_finish_event,
                  &mut event_cursor,
                ) {
                  suspend!();
                }
              } else if self.has_content_length {
                if self.content_length == 0 {
                  self.continue_without_data = true;
                  if !self.complete_message(
                    0,
                    active_events,
                    has_complete_events,
                    has_finish_event,
                    &mut event_cursor,
                  ) {
                    suspend!();
                  }
                } else {
                  move_to!(body_via_content_length);
                }
              } else if self.has_chunked_transfer_encoding {
                move_to!(chunk_header);
              } else {
                move_to!(body_with_no_length);
              }
            }
          }

          // RFC 9112 section 6.2
          STATE_BODY_VIA_CONTENT_LENGTH => {
            let expected = self.remaining_content_length;
            let mut to_consume = expected.min(available as u64);

            if max_body_payload != 0 {
              if body_payload_read >= max_body_payload {
                suspend!();
              }

              to_consume = to_consume.min(max_body_payload - body_payload_read);
            }

            if to_consume == 0 {
              suspend!();
            }

            let to_consume_usize = to_consume as usize;
            body_payload_read += to_consume;

            if to_consume < expected {
              event_with_range!(on_data, 0, to_consume_usize);
              self.remaining_content_length -= to_consume;
              advance!(to_consume_usize);
            } else {
              // Suspend before emitting or consuming any part of the final body segment.
              if event_cursor + EVENT_BODY_COMPLETION_SIZE + EVENT_ERROR_RESERVE > EVENTS_BUFFER_SIZE {
                suspend!();
              }
              event_with_range!(on_data, 0, to_consume_usize);
              event_with_range!(on_body, to_consume_usize, 0);

              self.remaining_content_length = 0;

              self.continue_without_data = true;

              if !self.complete_message(
                to_consume_usize,
                active_events,
                has_complete_events,
                has_finish_event,
                &mut event_cursor,
              ) {
                suspend!();
              }
              advance!(to_consume_usize);
            }
          }

          // RFC 9110 section 6.3 - Body with no length nor chunked encoding.
          // This is only allowed in responses.
          // Note that on_body can't and will not be called here as there is no way to
          // know when the response finishes.
          STATE_BODY_WITH_NO_LENGTH => {
            let mut to_consume = available as u64;

            if max_body_payload != 0 {
              if body_payload_read >= max_body_payload {
                suspend!();
              }

              to_consume = to_consume.min(max_body_payload - body_payload_read);
            }

            if to_consume == 0 {
              suspend!();
            }

            let to_consume_usize = to_consume as usize;
            body_payload_read += to_consume;

            event_with_range!(on_data, 0, to_consume_usize);
            advance!(to_consume_usize);
          }

          // RFC 9112 section 7.1
          STATE_CHUNK_HEADER => {
            match find_cr(data, available) {
              Some(cr) => {
                match ensure_valid_line(data, cr, available) {
                  MatchResult::Continue => {}
                  MatchResult::Suspend => {
                    suspend!();
                  }
                  MatchResult::Stop => {
                    fail!(UNEXPECTED_CHARACTER, "Expected CRLF");
                  }
                }

                let chunk_length_start = 0;
                // Note, the character is optional since chunk extensions are not required
                let chunk_length_end = match find_char(data, chunk_length_start, cr, b';') {
                  Some(index) => index,
                  None => cr,
                };

                if chunk_length_end == 0 {
                  fail!(UNEXPECTED_CHARACTER, "Invalid chunk length character");
                } else if chunk_length_end - chunk_length_start > 16 {
                  fail!(INVALID_CHUNK_SIZE, "Invalid chunk length size");
                }

                let mut i = chunk_length_start;
                let mut chunk_length = 0u64;
                while i < chunk_length_end {
                  let b = data[i];

                  let hex = if b.is_ascii_digit() {
                    (b - b'0') as u64
                  } else if (b'a'..=b'f').contains(&b) {
                    (b - b'a' + 10) as u64
                  } else if (b'A'..=b'F').contains(&b) {
                    (b - b'A' + 10) as u64
                  } else {
                    fail!(UNEXPECTED_CHARACTER, "Invalid chunk length character");
                  };

                  chunk_length = chunk_length * 16 + hex;
                  i += 1;
                }

                self.chunk_size = chunk_length;
                self.remaining_chunk_size = chunk_length;

                event_with_range!(
                  on_chunk_length,
                  chunk_length_start,
                  chunk_length_end - chunk_length_start
                );

                // There are extensions
                if chunk_length_end < cr {
                  advance!(chunk_length_end + 1);
                  move_to!(chunk_extensions);
                } else {
                  self.continue_without_data = true;

                  if self.chunk_size == 0 {
                    event_with_range!(on_chunk, 3, 0);
                    event_with_range!(on_body, 3, 0);
                    advance!(cr + 2);
                    move_to!(trailer);
                  } else {
                    advance!(cr + 2);
                    move_to!(chunk_data);
                  }
                }
              }
              None => {
                if available >= self.max_header_length {
                  fail!(UNEXPECTED_CHARACTER, "Chunk header too long");
                } else {
                  suspend!();
                }
              }
            }
          }

          STATE_CHUNK_EXTENSIONS => {
            match find_cr(data, available) {
              Some(cr) => {
                match ensure_valid_line(data, cr, available) {
                  MatchResult::Continue => {}
                  MatchResult::Suspend => {
                    suspend!();
                  }
                  MatchResult::Stop => {
                    fail!(UNEXPECTED_CHARACTER, "Expected CRLF");
                  }
                }

                let mut name_start = 0;
                // Find the first between = or ;
                let name_end_raw = find_char2(data, name_start, cr, b'=', b';').unwrap_or(cr);
                let mut name_end = name_end_raw;

                if !strip_ows(data, &mut name_start, &mut name_end, false) {
                  fail!(UNEXPECTED_CHARACTER, "Expected chunk extension name");
                }

                if !validate_token(data, name_start, name_end) {
                  fail!(UNEXPECTED_CHARACTER, "Invalid chunk extension name character");
                }

                // No value
                if name_end == cr || data[name_end_raw] == b';' {
                  event_with_range!(on_chunk_extension_name, name_start, name_end - name_start);

                  if name_end_raw == cr {
                    if self.chunk_size == 0 {
                      event_with_range!(on_body, 0, 0);
                      advance!(cr + 2);
                      move_to!(trailer);
                    } else {
                      advance!(cr + 2);
                      move_to!(chunk_data);
                    }
                  } else {
                    advance!(name_end_raw + 1);
                    move_to!(chunk_extensions);
                  }
                } else {
                  // Get the value
                  let mut value_start = name_end_raw + 1;
                  let mut value_end: usize;
                  let next_extension: usize;

                  // Strip OWS before the value
                  while value_start < cr && is_ws(data[value_start]) {
                    value_start += 1;
                  }

                  if value_start == cr {
                    fail!(UNEXPECTED_CHARACTER, "Expected chunk extension value");
                  }

                  // Quoted string
                  // RFC 9110 section 5.6.4
                  let mut quoted = false;
                  let quote_start = value_start;
                  if data[value_start] == b'"' {
                    quoted = true;
                    value_start += 1;
                    let mut quote_start = value_start;

                    loop {
                      match find_char(data, quote_start, cr, b'"') {
                        Some(index) => {
                          // Count consecutive backslashes immediately before the quote
                          let mut backslash_count = 0usize;
                          let mut i = index;

                          while i > quote_start && data[i - 1] == b'\\' {
                            backslash_count += 1;
                            i -= 1;
                          }

                          if backslash_count.is_multiple_of(2) {
                            // quote is not escaped
                            value_end = index;
                            break;
                          } else {
                            // quote is escaped, continue searching after it
                            quote_start = index + 1;
                          }
                        }
                        None => {
                          fail!(UNEXPECTED_CHARACTER, "Expected closing quote for chunk extension value");
                        }
                      };
                    }

                    if !validate_quoted_string(data, value_start, value_end) {
                      fail!(UNEXPECTED_CHARACTER, "Invalid chunk extension quoted value character");
                    }

                    next_extension = value_end + 1;
                  } else {
                    value_end = find_char(data, value_start, cr, b';').unwrap_or(cr);
                    next_extension = if value_end == cr { cr } else { value_end };

                    if !strip_ows(data, &mut value_start, &mut value_end, false) {
                      fail!(UNEXPECTED_CHARACTER, "Expected chunk extension value");
                    }

                    if value_start != value_end && !validate_token(data, value_start, value_end) {
                      fail!(UNEXPECTED_CHARACTER, "Invalid chunk extension value character");
                    }
                  }

                  event_with_range!(on_chunk_extension_name, name_start, name_end - name_start);

                  if quoted {
                    event_with_range!(on_chunk_extension_value, quote_start, value_end - quote_start + 1);
                  } else {
                    event_with_range!(on_chunk_extension_value, value_start, value_end - value_start);
                  }

                  let next_semicolon = find_char(data, next_extension, cr, b';').unwrap_or(cr);

                  let mut i = next_extension;
                  while i < next_semicolon {
                    if !is_ws(data[i]) {
                      fail!(UNEXPECTED_CHARACTER, "Invalid chunk extension character after value");
                    }
                    i += 1;
                  }

                  if next_semicolon < cr {
                    advance!(next_semicolon + 1);
                  } else {
                    if self.chunk_size == 0 {
                      event_with_range!(on_body, 0, 0);
                      advance!(cr + 2);
                      move_to!(trailer);
                    } else {
                      advance!(cr + 2);
                      move_to!(chunk_data);
                    }
                  }
                }
              }
              None => {
                // Given in chunk_header we already validated this, this should not happen.
                if available >= self.max_header_length {
                  fail!(UNEXPECTED_CHARACTER, "Chunk header too long");
                } else {
                  suspend!();
                }
              }
            }
          }

          STATE_CHUNK_DATA => {
            let expected = self.remaining_chunk_size;

            // No more data for this chunk, just wait for the CRLF
            if expected == 0 {
              if available < 2 {
                suspend!();
              } else if data[0] != b'\r' || data[1] != b'\n' {
                fail!(UNEXPECTED_CHARACTER, "Expected CRLF after chunk data");
              } else {
                advance!(2);
                move_to!(chunk_header);
              }
            } else {
              let mut to_consume = expected.min(available as u64);

              if max_body_payload != 0 {
                if body_payload_read >= max_body_payload {
                  suspend!();
                }

                to_consume = to_consume.min(max_body_payload - body_payload_read);
              }

              if to_consume == 0 {
                suspend!();
              }

              let to_consume_usize = to_consume as usize;
              body_payload_read += to_consume;

              event_with_range!(on_chunk, 0, to_consume_usize);
              event_with_range!(on_data, 0, to_consume_usize);

              if to_consume < expected {
                self.remaining_chunk_size -= to_consume;
              } else {
                self.remaining_chunk_size = 0;
              }

              advance!(to_consume_usize);
            }
          }

          // RFC 9112 section 7.1.2
          STATE_TRAILER => {
            match find_header_line_end(data.as_ptr(), available) {
              HeaderLineScanResult::Cr(cr) => {
                match ensure_valid_line(data, cr, available) {
                  MatchResult::Continue => {}
                  MatchResult::Suspend => {
                    suspend!();
                  }
                  MatchResult::Stop => {
                    fail!(UNEXPECTED_CHARACTER, "Expected CRLF");
                  }
                }

                // No more trailers or no trailers at all, message completed
                if cr == 0 {
                  // Keep the trailer notification and completion in the same batch.
                  if event_cursor + EVENT_TRAILER_COMPLETION_SIZE + EVENT_ERROR_RESERVE > EVENTS_BUFFER_SIZE {
                    suspend!();
                  }
                  event_with_range!(on_trailers, 2, 0);
                  self.continue_without_data = true;
                  if !self.complete_message(
                    2,
                    active_events,
                    has_complete_events,
                    has_finish_event,
                    &mut event_cursor,
                  ) {
                    suspend!();
                  }
                  advance!(2);
                  next!();
                }

                let trailer_name_start = 0;
                let trailer_name_end = match find_char(data, trailer_name_start, cr, b':') {
                  Some(index) if index > trailer_name_start => index,
                  _ => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid trailer field name character");
                  }
                };

                let mut trailer_value_start = trailer_name_end + 1;
                let mut trailer_value_end = cr;
                if has_trailer_value_event {
                  strip_ows_fast(data, &mut trailer_value_start, &mut trailer_value_end, true);
                }

                // Validate
                if !validate_token(data, trailer_name_start, trailer_name_end) {
                  fail!(UNEXPECTED_CHARACTER, "Invalid trailer field name character");
                }

                if has_trailer_name_event {
                  event_with_range!(
                    on_trailer_name,
                    trailer_name_start,
                    trailer_name_end - trailer_name_start
                  );
                }

                if has_trailer_value_event {
                  event_with_range!(
                    on_trailer_value,
                    trailer_value_start,
                    trailer_value_end - trailer_value_start
                  );
                }
                advance!(cr + 2);
              }
              HeaderLineScanResult::Invalid(invalid) => {
                match find_char(data, 0, invalid, b':') {
                  Some(_) => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid trailer field value character");
                  }
                  None => {
                    fail!(UNEXPECTED_CHARACTER, "Invalid trailer field name character");
                  }
                }
              }
              HeaderLineScanResult::Incomplete => {
                if available >= self.max_header_length {
                  fail!(UNEXPECTED_CHARACTER, "Trailer line too long");
                } else {
                  suspend!();
                }
              }
            }
          }

          // Return PAUSE makes this method idempotent without failing - In this state
          // all data is ignored since the connection is not in HTTP anymore
          STATE_TUNNEL => {
            suspend!();
          }

          _ => {
            fail!(UNEXPECTED_STATE, "Invalid state");
          }
        }
      }

      // Update the parser position
      if advanced > 0 {
        self.position += advanced;
        data = &data[advanced..];
        available -= advanced;

        #[cfg(any(debug_assertions, feature = "debug"))]
        if self.debug {
          eprintln!(
            "[milo_parser::debug] loop before processing: position={}, advanced={}, available={}, \
             continue_without_data={}",
            self.position, advanced, available, self.continue_without_data
          );
        }
      }

      // Notify the status change
      #[cfg(any(debug_assertions, feature = "debug"))]
      if previous_state != self.state {
        if active_events & EVENT_ACTIVE_ON_STATE_CHANGE != 0
          && !self.try_emit_event_range(
            &mut event_cursor,
            EVENT_STATE_CHANGE,
            self.position,
            self.state as usize,
          )
        {
          parsing = false;
          continue 'parser;
        }
        previous_state = self.state;
      }

      // Show the duration of the operation
      #[cfg(all(not(target_family = "wasm"), any(debug_assertions, feature = "debug")))]
      if self.debug {
        let duration = Instant::now().duration_since(last).as_nanos();

        if duration > 0 {
          eprintln!(
            "[milo_parser::debug] loop iteration ({:?}) completed in {} ns",
            self.state_str(),
            duration
          );
        }

        last = Instant::now();
      }
    }

    #[cfg(any(debug_assertions, feature = "debug"))]
    if self.debug {
      eprintln!("[milo_parser::debug] loop exit");
    }

    let consumed = self.position;
    self.parsed += consumed as u64;

    if self.manage_unconsumed {
      self.clear_unconsumed();

      // Use a boxed slice so the length also determines the allocation layout
      // when releasing retained input; Vec capacity is not stored in the ABI.
      if consumed < limit {
        let retained = data.to_vec().into_boxed_slice();
        self.unconsumed_len = retained.len();
        self.unconsumed = Box::into_raw(retained) as *const c_uchar;
      }
    }

    #[cfg(all(not(target_family = "wasm"), any(debug_assertions, feature = "debug")))]
    if self.debug {
      let duration = Instant::now().duration_since(start).as_nanos();

      if duration > 0 {
        eprintln!(
          "[milo_parser::debug] parse ({:?}, consumed {} of {}) completed in {} ns",
          self.state_str(),
          consumed,
          limit,
          duration
        );
      }
    }

    // Append only errors raised by this call, after all preceding events.
    if failed && active_events & EVENT_ACTIVE_ON_ERROR != 0 {
      self.try_emit_event_error(&mut event_cursor);
    }

    if has_active_events {
      unsafe {
        *self.events.add(event_cursor) = EVENT_END;
      }
    }

    if self.active_callbacks != 0 {
      self.invoke_callbacks();
    }

    // Return the number of consumed bytes
    consumed
  }

  /// Completes the current message without consuming more input.
  ///
  /// This is intended for integrations that consume opaque body bytes outside
  /// Milo after headers were parsed, then need Milo to perform the normal
  /// completion transition and emit completion events.
  pub fn complete(&mut self) {
    match self.state {
      STATE_BODY_DECISION
      | STATE_TUNNEL
      | STATE_BODY_VIA_CONTENT_LENGTH
      | STATE_BODY_WITH_NO_LENGTH
      | STATE_CHUNK_HEADER
      | STATE_TRAILER => {}
      _ => {
        self.fail(ERROR_UNEXPECTED_STATE, "Invalid state");
        return;
      }
    }

    let active_events = self.active_events | self.active_callbacks;
    let has_complete_events = active_events & (EVENT_ACTIVE_ON_MESSAGE_COMPLETE | EVENT_ACTIVE_ON_RESET) != 0;
    let has_finish_event = active_events & EVENT_ACTIVE_ON_FINISH != 0;
    let mut event_cursor = 0usize;

    if !self.complete_message(
      0,
      active_events,
      has_complete_events,
      has_finish_event,
      &mut event_cursor,
    ) {
      self.paused = true;
    }

    unsafe {
      *self.events.add(event_cursor) = EVENT_END;
    }
  }

  // RFC 9110 section 6.4.1 - Message completed.
  #[inline(always)]
  fn complete_message(
    &mut self,
    offset: usize,
    active_events: u64,
    has_complete_events: bool,
    has_finish_event: bool,
    event_cursor: &mut usize,
  ) -> bool {
    // Never publish a partial completion group that would be replayed on retry.
    if *event_cursor + EVENT_COMPLETION_SIZE + EVENT_ERROR_RESERVE > EVENTS_BUFFER_SIZE {
      return false;
    }
    if has_complete_events {
      if (active_events & EVENT_ACTIVE_ON_MESSAGE_COMPLETE != 0
        && !self.try_emit_event_range(event_cursor, EVENT_MESSAGE_COMPLETE, self.position + offset, 0))
        || (active_events & EVENT_ACTIVE_ON_RESET != 0
          && !self.try_emit_event_range(event_cursor, EVENT_RESET, self.position + offset, 0))
      {
        return false;
      }
    }

    self.continue_without_data = false;
    self.skip_body = false;

    if self.has_upgrade && self.is_request {
      move_to!(tunnel);
    } else if self.has_connection_close {
      if has_finish_event && !self.try_emit_event_range(event_cursor, EVENT_FINISH, self.position, 0) {
        return false;
      }
      move_to!(finish);
    } else {
      move_to!(start);
    }

    true
  }
}
