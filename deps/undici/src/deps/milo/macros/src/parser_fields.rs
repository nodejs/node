use core::mem::offset_of;

use proc_macro2::TokenStream;
use quote::{format_ident, quote};

type WasmPointer = u32;
type WasmUsize = u32;

// Keep this in sync with parser::Parser when compiled with target_family =
// "wasm".
#[repr(C)]
struct ParserStub {
  max_start_line_length: WasmUsize,
  max_header_length: WasmUsize,
  max_body_payload: u64,
  autodetect: bool,
  is_request: bool,
  suspend_after_headers: bool,
  manage_unconsumed: bool,
  continue_without_data: bool,
  is_connect: bool,
  skip_body: bool,
  debug: bool,
  parsed: u64,
  position: WasmUsize,
  state: u8,
  paused: bool,
  error_code: u8,
  content_length: u64,
  chunk_size: u64,
  remaining_content_length: u64,
  remaining_chunk_size: u64,
  status: u32,
  method: u8,
  has_content_length: bool,
  has_transfer_encoding: bool,
  has_chunked_transfer_encoding: bool,
  has_connection_close: bool,
  has_connection_upgrade: bool,
  has_upgrade: bool,
  has_trailers: bool,
  active_callbacks: u64,
  active_events: u64,
  ptr: WasmPointer,
  error_description: [u8; 255],
  unconsumed: WasmPointer,
  unconsumed_len: WasmUsize,
  error_description_len: u8,
  events: WasmPointer,
}

pub const FIELDS: &[(&str, usize)] = &[
  ("MAX_START_LINE_LENGTH", offset_of!(ParserStub, max_start_line_length)),
  ("MAX_HEADER_LENGTH", offset_of!(ParserStub, max_header_length)),
  ("MAX_BODY_PAYLOAD", offset_of!(ParserStub, max_body_payload)),
  ("AUTODETECT", offset_of!(ParserStub, autodetect)),
  ("IS_REQUEST", offset_of!(ParserStub, is_request)),
  ("SUSPEND_AFTER_HEADERS", offset_of!(ParserStub, suspend_after_headers)),
  ("MANAGE_UNCONSUMED", offset_of!(ParserStub, manage_unconsumed)),
  ("CONTINUE_WITHOUT_DATA", offset_of!(ParserStub, continue_without_data)),
  ("IS_CONNECT", offset_of!(ParserStub, is_connect)),
  ("SKIP_BODY", offset_of!(ParserStub, skip_body)),
  ("DEBUG", offset_of!(ParserStub, debug)),
  ("PARSED", offset_of!(ParserStub, parsed)),
  ("POSITION", offset_of!(ParserStub, position)),
  ("STATE", offset_of!(ParserStub, state)),
  ("PAUSED", offset_of!(ParserStub, paused)),
  ("ERROR_CODE", offset_of!(ParserStub, error_code)),
  ("CONTENT_LENGTH", offset_of!(ParserStub, content_length)),
  ("CHUNK_SIZE", offset_of!(ParserStub, chunk_size)),
  (
    "REMAINING_CONTENT_LENGTH",
    offset_of!(ParserStub, remaining_content_length),
  ),
  ("REMAINING_CHUNK_SIZE", offset_of!(ParserStub, remaining_chunk_size)),
  ("STATUS", offset_of!(ParserStub, status)),
  ("METHOD", offset_of!(ParserStub, method)),
  ("HAS_CONTENT_LENGTH", offset_of!(ParserStub, has_content_length)),
  ("HAS_TRANSFER_ENCODING", offset_of!(ParserStub, has_transfer_encoding)),
  (
    "HAS_CHUNKED_TRANSFER_ENCODING",
    offset_of!(ParserStub, has_chunked_transfer_encoding),
  ),
  ("HAS_CONNECTION_CLOSE", offset_of!(ParserStub, has_connection_close)),
  ("HAS_CONNECTION_UPGRADE", offset_of!(ParserStub, has_connection_upgrade)),
  ("HAS_UPGRADE", offset_of!(ParserStub, has_upgrade)),
  ("HAS_TRAILERS", offset_of!(ParserStub, has_trailers)),
  ("ACTIVE_CALLBACKS", offset_of!(ParserStub, active_callbacks)),
  ("ACTIVE_EVENTS", offset_of!(ParserStub, active_events)),
  ("PTR", offset_of!(ParserStub, ptr)),
  ("ERROR_DESCRIPTION", offset_of!(ParserStub, error_description)),
  ("UNCONSUMED", offset_of!(ParserStub, unconsumed)),
  ("UNCONSUMED_LEN", offset_of!(ParserStub, unconsumed_len)),
  ("ERROR_DESCRIPTION_LEN", offset_of!(ParserStub, error_description_len)),
  ("EVENTS", offset_of!(ParserStub, events)),
];

pub fn generate_constants() -> TokenStream {
  let constants = FIELDS.iter().map(|(name, offset)| {
    let ident = format_ident!("PARSER_FIELD_{}", name);

    quote! {
      #[cfg(target_family = "wasm")]
      pub const #ident: usize = #offset;
    }
  });

  quote! {
    #(#constants)*
  }
}

#[allow(dead_code)]
fn main() {
  let constants = FIELDS
    .iter()
    .map(|(name, offset)| format!("\"PARSER_FIELD_{name}\":{offset}"))
    .collect::<Vec<_>>()
    .join(",");

  println!("{{{}}}", constants);
}
