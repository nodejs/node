use proc_macro::TokenStream;
use quote::{format_ident, quote};

/// Generates all parser callbacks.
pub fn generate_callbacks(callbacks: &[String]) -> TokenStream {
  let callbacks: Vec<_> = callbacks
    .iter()
    .filter(|x| x.as_str() != "on_headers")
    .map(|x| format_ident!("{}", x))
    .collect();
  let replay_arms = callbacks
    .iter()
    .filter(|callback| callback.to_string() != "on_error")
    .map(|callback| {
      let callback_name = callback.to_string();
      let event_const = format_ident!(
        "EVENT_{}",
        callback_name
          .strip_prefix("on_")
          .unwrap_or(&callback_name)
          .to_uppercase()
      );
      let active_const = format_ident!("CALLBACK_ACTIVE_{}", callback_name.to_uppercase());

      quote! {
        #event_const => {
          let at = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 1) as *const u32) }.to_le() as usize;
          let len = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 5) as *const u32) }.to_le() as usize;
          if self.active_callbacks & #active_const != 0 {
            unsafe { #callback(self.ptr, at, len); }
          }
          cursor += EVENT_RANGE_SIZE;
        }
      }
    });

  TokenStream::from(quote! {
    #[cfg(target_family = "wasm")]
    #[link(wasm_import_module = "env")]
    unsafe extern "C" {
      #(fn #callbacks(parser: *mut c_void, _at: usize, _len: usize);)*
      fn on_headers(
        parser: *mut c_void,
        at: usize,
        method_or_status: u32,
        should_keep_alive: bool,
        should_upgrade: bool,
        has_trailers: bool,
        body_kind: u8,
        content_length: f64,
      );

      #[cfg(any(debug_assertions, feature = "debug"))]
      fn logger(message: u64);
    }

    #[cfg(all(any(debug_assertions, feature = "debug"), target_family = "wasm"))]
    #[unsafe(no_mangle)]
    pub fn __start() {
      std::panic::set_hook(Box::new(|panic_info| {
        debug(format!("WebAssembly panicked: {:#?}", panic_info));
      }));
    }

    #[cfg(target_family = "wasm")]
    impl Parser {
      #[inline]
      fn invoke_callbacks(&mut self) {
        let mut cursor = 0usize;

        loop {
          let event_type = unsafe { *self.events.add(cursor) };

          match event_type {
            EVENT_END => break,
            EVENT_ERROR => {
              let at = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 1) as *const u32) }.to_le() as usize;
              if self.active_callbacks & CALLBACK_ACTIVE_ON_ERROR != 0 {
                unsafe { on_error(self.ptr, at, 0); }
              }
              cursor += EVENT_ERROR_SIZE;
            }
            EVENT_HEADERS => {
              let at = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 1) as *const u32) }.to_le() as usize;
              let method_or_status = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 5) as *const u16) }.to_le() as u32;
              let should_keep_alive = unsafe { *self.events.add(cursor + 7) } != 0;
              let should_upgrade = unsafe { *self.events.add(cursor + 8) } != 0;
              let has_trailers = unsafe { *self.events.add(cursor + 9) } != 0;
              let body_kind = unsafe { *self.events.add(cursor + 10) };
              let content_length = unsafe { core::ptr::read_unaligned(self.events.add(cursor + 11) as *const u64) }.to_le() as f64;

              if self.active_callbacks & CALLBACK_ACTIVE_ON_HEADERS != 0 {
                unsafe {
                  on_headers(
                    self.ptr,
                    at,
                    method_or_status,
                    should_keep_alive,
                    should_upgrade,
                    has_trailers,
                    body_kind,
                    content_length,
                  );
                }
              }
              cursor += EVENT_METADATA_SIZE;
            }
            #(#replay_arms)*
            _ => break,
          }
        }
      }
    }
  })
}
