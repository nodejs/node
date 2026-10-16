use core::ffi::{c_uchar, c_void};
use std::slice;

use crate::Parser;

#[cfg(any(debug_assertions, feature = "debug"))]
pub fn debug(message: String) { unsafe { crate::logger(((message.as_ptr() as u64) << 32) + message.len() as u64) } }

/// Returns if debug informations are available in this build.
#[unsafe(no_mangle)]
pub fn milo_has_debug() -> bool { cfg!(any(debug_assertions, feature = "debug")) }

#[unsafe(no_mangle)]
pub fn alloc(len: usize) -> *mut c_void {
  let buffer = Vec::with_capacity(len);
  let (ptr, _, _) = { buffer.into_raw_parts() };
  ptr as *mut c_void
}

#[unsafe(no_mangle)]
pub fn dealloc(ptr: *mut c_void, len: usize) {
  if ptr.is_null() {
    return;
  }

  unsafe {
    let _ = Vec::from_raw_parts(ptr, len, len);
  }
}

/// Creates a new parser.
#[unsafe(no_mangle)]
pub fn create() -> *mut c_void {
  let ptr = Box::into_raw(Box::new(Parser::new())) as *mut c_void;

  // Recreate the parser from the box to assign the reference to itself
  let mut parser = unsafe { Box::from_raw(ptr as *mut Parser) };
  parser.ptr = ptr;
  let _ = Box::into_raw(parser);

  ptr
}

/// Destroys a parser.
#[unsafe(no_mangle)]
pub fn destroy(parser: *mut c_void) {
  if parser.is_null() {
    return;
  }

  unsafe {
    let _ = Box::from_raw(parser as *mut Parser);
  }
}

/// Resets a parser. The second parameters specifies if to also reset the
/// parsed counter.
#[unsafe(no_mangle)]
pub fn reset(parser: *mut c_void, keep_parsed: bool) { unsafe { (*(parser as *mut Parser)).reset(keep_parsed) } }

/// Clears all values in the parser.
///
/// Persisted fields, unconsumed data and the position are not cleared.
#[unsafe(no_mangle)]
pub fn clear(parser: *mut c_void) { unsafe { (*(parser as *mut Parser)).clear() } }

// Parses a slice of characters. It returns the number of consumed characters.
#[unsafe(no_mangle)]
pub fn parse(parser: *mut c_void, data: *const c_uchar, limit: usize) -> usize {
  unsafe { (*(parser as *mut Parser)).parse(data, limit) }
}

/// Pauses the parser. It will have to be resumed via `resume`.
#[unsafe(no_mangle)]
pub fn pause(parser: *mut c_void) { unsafe { (*(parser as *mut Parser)).pause() } }

/// Resumes the parser.
#[unsafe(no_mangle)]
pub fn resume(parser: *mut c_void) { unsafe { (*(parser as *mut Parser)).resume() } }

/// Completes the current message without consuming more input.
#[unsafe(no_mangle)]
pub fn complete(parser: *mut c_void) { unsafe { (*(parser as *mut Parser)).complete() } }

/// Marks the parser as finished. Any new data received via `parse` will
/// put the parser in the error state.
#[unsafe(no_mangle)]
pub fn finish(parser: *mut c_void) { unsafe { (*(parser as *mut Parser)).finish() } }

/// Marks the parser as failed.
#[unsafe(no_mangle)]
pub fn fail(parser: *mut c_void, code: u8, description_ptr: *const c_uchar, description_len: usize) {
  unsafe {
    (*(parser as *mut Parser)).fail(
      code,
      std::str::from_utf8_unchecked(slice::from_raw_parts(description_ptr, description_len)),
    );
  }
}

// Generates a getter.
// pub fn wasm_getter(input: TokenStream) -> TokenStream {
// let snake_matcher = Regex::new(r"([A-Z])").unwrap();
//
// let definition = parse_macro_input!(input as Property);
// let property = definition.property;
// let fn_name = format_ident!(
// "{}",
// snake_matcher.replace_all(&definition.getter.to_string().as_str(), |captures:
// &Captures| { format!("_{}", captures[1].to_lowercase())
// })
// );
//
// let return_type = definition.r#type;
//
// TokenStream::from(quote! {
// Gets the parser #property.
// #[unsafe(no_mangle)]
// pub fn #fn_name(parser: *const c_void) -> #return_type { unsafe { (*(parser
// as *const Parser)).#property } } })
// }

// Getters
// Get the parser autodetect property.
#[unsafe(no_mangle)]
pub fn is_autodetect(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).autodetect } }

// Get the parser is_request property.
#[unsafe(no_mangle)]
pub fn is_request(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).is_request } }

// Get the parser debug property.
#[unsafe(no_mangle)]
pub fn is_debug(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).debug } }

// Get the parser paused property.
#[unsafe(no_mangle)]
pub fn is_paused(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).paused } }

// Get the parser manage_unconsumed property.
#[unsafe(no_mangle)]
pub fn should_manage_unconsumed(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).manage_unconsumed }
}

// Get the parser suspend_after_headers property.
#[unsafe(no_mangle)]
pub fn should_suspend_after_headers(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).suspend_after_headers }
}

// Get the parser max_start_line_length property.
#[unsafe(no_mangle)]
pub fn get_max_start_line_length(parser: *const c_void) -> usize {
  unsafe { (*(parser as *const Parser)).max_start_line_length }
}

// Get the parser max_header_length property.
#[unsafe(no_mangle)]
pub fn get_max_header_length(parser: *const c_void) -> usize {
  unsafe { (*(parser as *const Parser)).max_header_length }
}

// Get the parser max_body_payload property.
#[unsafe(no_mangle)]
pub fn get_max_body_payload(parser: *const c_void) -> u64 { unsafe { (*(parser as *const Parser)).max_body_payload } }

// Get the parser continue_without_data property.
#[unsafe(no_mangle)]
pub fn should_continue_without_data(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).continue_without_data }
}

// Get the parser is_connect property.
#[unsafe(no_mangle)]
pub fn is_connect(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).is_connect } }

// Get the parser skip_body property.
#[unsafe(no_mangle)]
pub fn should_skip_body(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).skip_body } }

// Get the parser state property.
#[unsafe(no_mangle)]
pub fn get_state(parser: *const c_void) -> u8 { unsafe { (*(parser as *const Parser)).state } }

// Get the parser position property.
#[unsafe(no_mangle)]
pub fn get_position(parser: *const c_void) -> usize { unsafe { (*(parser as *const Parser)).position } }

// Get the parser parsed property.
#[unsafe(no_mangle)]
pub fn get_parsed(parser: *const c_void) -> u64 { unsafe { (*(parser as *const Parser)).parsed } }

// Get the parser error_code property.
#[unsafe(no_mangle)]
pub fn get_error_code(parser: *const c_void) -> u8 { unsafe { (*(parser as *const Parser)).error_code } }

// Get the parser method property.
#[unsafe(no_mangle)]
pub fn get_method(parser: *const c_void) -> u8 { unsafe { (*(parser as *const Parser)).method } }

// Get the parser status property.
#[unsafe(no_mangle)]
pub fn get_status(parser: *const c_void) -> u32 { unsafe { (*(parser as *const Parser)).status } }

// Get the parser has_connection_close property.
#[unsafe(no_mangle)]
pub fn has_connection_close(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).has_connection_close }
}

// Get the parser has_connection_upgrade property.
#[unsafe(no_mangle)]
pub fn has_connection_upgrade(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).has_connection_upgrade }
}

// Get the parser content_length property.
#[unsafe(no_mangle)]
pub fn get_content_length(parser: *const c_void) -> u64 { unsafe { (*(parser as *const Parser)).content_length } }

// Get the parser chunk_size property.
#[unsafe(no_mangle)]
pub fn get_chunk_size(parser: *const c_void) -> u64 { unsafe { (*(parser as *const Parser)).chunk_size } }

// Get the parser remaining_content_length property.
#[unsafe(no_mangle)]
pub fn get_remaining_content_length(parser: *const c_void) -> u64 {
  unsafe { (*(parser as *const Parser)).remaining_content_length }
}

// Get the parser remaining_chunk_size property.
#[unsafe(no_mangle)]
pub fn get_remaining_chunk_size(parser: *const c_void) -> u64 {
  unsafe { (*(parser as *const Parser)).remaining_chunk_size }
}

// Get the parser has_content_length property.
#[unsafe(no_mangle)]
pub fn has_content_length(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).has_content_length } }

// Get the parser has_transfer_encoding property.
#[unsafe(no_mangle)]
pub fn has_transfer_encoding(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).has_transfer_encoding }
}

// Get the parser has_chunked_transfer_encoding property.
#[unsafe(no_mangle)]
pub fn has_chunked_transfer_encoding(parser: *const c_void) -> bool {
  unsafe { (*(parser as *const Parser)).has_chunked_transfer_encoding }
}

// Get the parser has_upgrade property.
#[unsafe(no_mangle)]
pub fn has_upgrade(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).has_upgrade } }

// Get the parser has_trailers property.
#[unsafe(no_mangle)]
pub fn has_trailers(parser: *const c_void) -> bool { unsafe { (*(parser as *const Parser)).has_trailers } }

/// Gets the parser callback error description, if any. This is meant for
/// internal use.
#[unsafe(no_mangle)]
pub fn get_error_description_raw(parser: *mut c_void) -> u64 {
  let parser = unsafe { &(*(parser as *const Parser)) };

  let ptr = parser.error_description.as_ptr() as u64;
  let len = parser.error_description_len as u64;

  (ptr << 32) + len
}

#[unsafe(no_mangle)]
pub fn set_should_autodetect(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).autodetect = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_is_request(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).is_request = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_should_manage_unconsumed(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).manage_unconsumed = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_should_suspend_after_headers(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).suspend_after_headers = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_max_start_line_length(parser: *mut c_void, value: usize) {
  unsafe {
    (*(parser as *mut Parser)).max_start_line_length = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_max_header_length(parser: *mut c_void, value: usize) {
  unsafe {
    (*(parser as *mut Parser)).max_header_length = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_max_body_payload(parser: *mut c_void, value: u64) {
  unsafe {
    (*(parser as *mut Parser)).max_body_payload = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_should_continue_without_data(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).continue_without_data = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_should_skip_body(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).skip_body = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_is_connect(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).is_connect = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_debug(parser: *mut c_void, value: bool) {
  unsafe {
    (*(parser as *mut Parser)).debug = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_active_callbacks(parser: *mut c_void, value: u64) {
  unsafe {
    (*(parser as *mut Parser)).active_callbacks = value;
  }
}

#[unsafe(no_mangle)]
pub fn set_active_events(parser: *mut c_void, value: u64) {
  unsafe {
    (*(parser as *mut Parser)).active_events = value;
  }
}
