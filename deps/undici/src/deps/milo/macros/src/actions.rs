use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::{Expr, Ident, parse_macro_input};

use crate::structs::{EventRequest, FailureRequest};

/// Emits an event carrying an input range.
pub fn event_with_range(input: TokenStream) -> TokenStream {
  let definition = parse_macro_input!(input as EventRequest);
  let callback = &definition.identifier;
  let callback_const = format_ident!("CALLBACK_{}", callback.to_string().to_uppercase());
  let event_type = quote! { #callback_const + 1 };
  let bitmask = format_ident!("EVENT_ACTIVE_{}", definition.identifier.to_string().to_uppercase());
  let offset = definition.offset.as_ref().expect("event_with_range requires offset");
  let length = definition.length.as_ref().expect("event_with_range requires length");
  let needed = quote! { EVENT_RANGE_SIZE };
  let emit = quote! {
    let at = (self.position + #offset) as u32;
    let len = (#length) as u32;
    unsafe {
      *self.events.add(event_cursor) = #event_type;
      core::ptr::write_unaligned(
        self.events.add(event_cursor + 1) as *mut u32,
        at.to_le(),
      );
      core::ptr::write_unaligned(
        self.events.add(event_cursor + 5) as *mut u32,
        len.to_le(),
      );
    }
  };

  TokenStream::from(quote! {
    if active_events & #bitmask != 0 {
      // Reserve space for a later error event and the batch terminator.
      if event_cursor + #needed + EVENT_ERROR_RESERVE <= EVENTS_BUFFER_SIZE {
        #emit
        event_cursor += #needed;
      } else {
        suspend!();
      }
    }
  })
}

/// Emits an error event.
pub fn event_with_error(input: TokenStream) -> TokenStream {
  let definition = parse_macro_input!(input as EventRequest);
  let callback = &definition.identifier;
  let callback_const = format_ident!("CALLBACK_{}", callback.to_string().to_uppercase());
  let event_type = quote! { #callback_const + 1 };
  let bitmask = format_ident!("EVENT_ACTIVE_{}", definition.identifier.to_string().to_uppercase());
  let needed = quote! { EVENT_ERROR_SIZE };
  let emit = quote! {
    let at = self.position as u32;
    unsafe {
      *self.events.add(event_cursor) = #event_type;
      core::ptr::write_unaligned(
        self.events.add(event_cursor + 1) as *mut u32,
        at.to_le(),
      );
      *self.events.add(event_cursor + 5) = self.error_code;
    }
  };

  TokenStream::from(quote! {
    if active_events & #bitmask != 0 {
      if event_cursor + #needed + EVENT_END_SIZE <= EVENTS_BUFFER_SIZE {
        #emit
        event_cursor += #needed;
      } else {
        suspend!();
      }
    }
  })
}

/// Emits an event carrying parsed metadata.
pub fn event_with_metadata(input: TokenStream) -> TokenStream {
  let definition = parse_macro_input!(input as EventRequest);
  let callback = &definition.identifier;
  let callback_const = format_ident!("CALLBACK_{}", callback.to_string().to_uppercase());
  let event_type = quote! { #callback_const + 1 };
  let bitmask = format_ident!("EVENT_ACTIVE_{}", definition.identifier.to_string().to_uppercase());
  let offset = definition.offset.unwrap_or_else(|| syn::parse_quote! { 0 });
  let needed = quote! { EVENT_METADATA_SIZE };
  let emit = quote! {
    let at = (self.position + #offset) as u32;
    let status_or_method = if self.is_request { self.method as u16 } else { self.status as u16 };
    let body_kind = if self.has_content_length { 0u8 } else if self.has_chunked_transfer_encoding { 1u8 } else { 2u8 };
    let should_keep_alive = (!self.has_connection_close) as u8;
    // Responses switch protocols only with status 101; requests can propose an upgrade.
    let should_upgrade = (self.has_upgrade && self.has_connection_upgrade && (self.is_request || self.status == 101)) as u8;
    let has_trailers = self.has_trailers as u8;
    let content_length = if self.has_content_length { self.content_length } else { 0 };

    unsafe {
      *self.events.add(event_cursor) = #event_type;
      core::ptr::write_unaligned(
        self.events.add(event_cursor + 1) as *mut u32,
        at.to_le(),
      );
      core::ptr::write_unaligned(
        self.events.add(event_cursor + 5) as *mut u16,
        status_or_method.to_le(),
      );
      *self.events.add(event_cursor + 7) = should_keep_alive;
      *self.events.add(event_cursor + 8) = should_upgrade;
      *self.events.add(event_cursor + 9) = has_trailers;
      *self.events.add(event_cursor + 10) = body_kind;
      core::ptr::write_unaligned(
        self.events.add(event_cursor + 11) as *mut u64,
        content_length.to_le(),
      );
    }
  };

  TokenStream::from(quote! {
    if active_events & #bitmask != 0 {
      // Reserve space for a later error event and the batch terminator.
      if event_cursor + #needed + EVENT_ERROR_RESERVE <= EVENTS_BUFFER_SIZE {
        #emit
        event_cursor += #needed;
      } else {
        suspend!();
      }
    }
  })
}

// Marks a certain number of characters as used.
pub fn advance(input: TokenStream) -> TokenStream {
  let len = parse_macro_input!(input as Expr);

  TokenStream::from(quote! { advanced += #len; })
}

/// Moves the parser to a new state.
pub fn move_to(input: TokenStream) -> TokenStream {
  let definition = parse_macro_input!(input as Ident);
  let state = format_ident!("STATE_{}", definition.to_string().to_uppercase());

  TokenStream::from(quote! {
     if self.state != STATE_ERROR {
       self.state = #state;
     }
  })
}

/// Go to the next iteration of the parser
pub fn next() -> TokenStream { TokenStream::from(quote! { break 'state; }) }

/// Marks the parser as suspended, waiting for more data.
pub fn suspend() -> TokenStream {
  TokenStream::from(quote! {
    parsing = false;
    break 'state;
  })
}

/// Marks the parsing a failed, setting a error code and and error message.
pub fn fail(input: TokenStream) -> TokenStream {
  let definition = parse_macro_input!(input as FailureRequest);
  let error = format_ident!("ERROR_{}", definition.error);
  let message = definition.message;

  TokenStream::from(quote! {
    self.set_error(#error, #message);
    failed = true;
    break 'parser;
  })
}
