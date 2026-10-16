use proc_macro::TokenStream;
use quote::{format_ident, quote};

/// Generates all parser callbacks.
pub fn generate_callbacks(callbacks: &[String]) -> TokenStream {
  let callbacks: Vec<_> = callbacks.iter().map(|x| format_ident!("{}", x)).collect();
  let replay_arms = callbacks.iter().map(|callback| {
    let callback_name = callback.to_string();
    let event_const = format_ident!(
      "EVENT_{}",
      callback_name
        .strip_prefix("on_")
        .unwrap_or(&callback_name)
        .to_uppercase()
    );
    let active_const = format_ident!("CALLBACK_ACTIVE_{}", callback_name.to_uppercase());

    if callback_name == "on_error" {
      quote! {
        #event_const => {
          let at = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 1) as *const u32) }.to_le() as usize;
          if self.active_callbacks & #active_const != 0 {
            (self.callbacks.#callback)(self, at, 0);
          }
          cursor += EVENT_ERROR_SIZE;
        }
      }
    } else if callback_name == "on_headers" {
      quote! {
        #event_const => {
          let at = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 1) as *const u32) }.to_le() as usize;
          if self.active_callbacks & #active_const != 0 {
            (self.callbacks.#callback)(self, at, 0);
          }
          cursor += EVENT_METADATA_SIZE;
        }
      }
    } else {
      quote! {
        #event_const => {
          let at = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 1) as *const u32) }.to_le() as usize;
          let len = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 5) as *const u32) }.to_le() as usize;
          if self.active_callbacks & #active_const != 0 {
            (self.callbacks.#callback)(self, at, len);
          }
          cursor += EVENT_RANGE_SIZE;
        }
      }
    }
  });

  TokenStream::from(quote! {
    #[cfg(not(target_family = "wasm"))]
    fn noop_internal(_parser: &mut Parser, _at: usize, _len: usize) {}

    #[cfg(not(target_family = "wasm"))]
    #[repr(C)]
    #[derive(Clone, Debug)]
    pub struct ParserCallbacks {
      #( pub #callbacks: Callback),*
    }

    #[cfg(not(target_family = "wasm"))]
    impl ParserCallbacks {
      fn new() -> ParserCallbacks {
        ParserCallbacks {
          #( #callbacks: noop_internal ),*
        }
      }
    }

    #[cfg(not(target_family = "wasm"))]
    impl Parser {
      #[inline]
      fn invoke_callbacks(&mut self) {
        let mut cursor = 0usize;

        loop {
          let event_type = unsafe { *self.events.add(cursor) };

          match event_type {
            EVENT_END => break,
            #(#replay_arms)*
            _ => break,
          }
        }
      }
    }
  })
}
