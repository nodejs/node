use proc_macro::TokenStream;
use quote::{format_ident, quote};
use regex::{Captures, Regex};
use syn::{Arm, ItemConst, parse_str};

use crate::{native, parser_fields, wasm};

fn init_constants() -> (Vec<String>, Vec<String>, Vec<String>, Vec<String>) {
  let methods = serde_yaml::from_str(include_str!("../constants/methods.yml")).unwrap();
  let errors = serde_yaml::from_str(include_str!("../constants/errors.yml")).unwrap();
  let callbacks = serde_yaml::from_str(include_str!("../constants/callbacks.yml")).unwrap();
  let states = serde_yaml::from_str(include_str!("../constants/states.yml")).unwrap();

  (methods, errors, callbacks, states)
}

fn generate_constants_internal(
  items: &[String],
  prefix: &str,
  start: usize,
  strip_prefix: Option<&str>,
) -> Vec<ItemConst> {
  let mut consts: Vec<ItemConst> = Vec::new();
  let mut bytes: Vec<&[u8]> = vec![];

  for (i, x) in items.iter().enumerate() {
    let value = strip_prefix.and_then(|prefix| x.strip_prefix(prefix)).unwrap_or(x);
    let uppercased = value.to_uppercase();
    let name = uppercased.replace('-', "_");
    bytes.push(x.as_bytes());

    consts.push(parse_str::<ItemConst>(&format!("pub const {}_{}: u8 = {};", prefix, name, i + start)).unwrap());
  }

  consts
}

fn generate_bitmask(items: &[String], prefix: &str) -> Vec<ItemConst> {
  let mut consts: Vec<ItemConst> = Vec::new();
  let mut all = 0u64;

  consts.push(parse_str::<ItemConst>(&format!("pub const {}_NONE: u64 = 0;", prefix)).unwrap());

  for (i, x) in items.iter().enumerate() {
    let uppercased = x.to_uppercase();
    let name = uppercased.replace('-', "_");
    let bit = 1 << i;
    all |= bit;

    consts.push(parse_str::<ItemConst>(&format!("pub const {}_{}: u64 = {};", prefix, name, bit)).unwrap());
  }

  consts.push(parse_str::<ItemConst>(&format!("pub const {}_ALL: u64 = {};", prefix, all)).unwrap());

  consts
}

fn generate_table<F>(validator: F) -> Vec<bool>
where
  F: Fn(u16) -> bool,
{
  // Precompute byte validators so the parser can use table lookups in hot paths.
  (0..=255).map(validator).collect()
}

/// Generates all parser constants.
fn generate_constants(methods: &[String], errors: &[String], callbacks: &[String], states: &[String]) -> TokenStream {
  let methods_consts = generate_constants_internal(methods, "METHOD", 0, None);
  let states_consts = generate_constants_internal(states, "STATE", 0, None);
  let errors_consts = generate_constants_internal(errors, "ERROR", 0, None);
  let callbacks_consts = generate_constants_internal(callbacks, "CALLBACK", 0, None);
  let callbacks_bitmask = generate_bitmask(callbacks, "CALLBACK_ACTIVE");
  let event_bitmask = generate_bitmask(callbacks, "EVENT_ACTIVE");
  let event_consts = generate_constants_internal(callbacks, "EVENT", 1, Some("on_"));
  let parser_field_offsets = parser_fields::generate_constants();
  let token_table = generate_table(|byte| {
    (0x30..=0x39).contains(&byte)
      || (0x41..=0x5a).contains(&byte)
      || (0x61..=0x7a).contains(&byte)
      || matches!(
        byte as u8,
        b'!' | b'#' | b'$' | b'%' | b'&' | b'\'' | b'*' | b'+' | b'-' | b'.' | b'^' | b'_' | b'`' | b'|' | b'~'
      )
  });
  let url_table = generate_table(|byte| {
    (0x30..=0x39).contains(&byte)
      || (0x41..=0x5a).contains(&byte)
      || (0x61..=0x7a).contains(&byte)
      || matches!(
        byte as u8,
        b'-'
          | b'.'
          | b'_'
          | b'~'
          | b':'
          | b'/'
          | b'?'
          | b'['
          | b']'
          | b'@'
          | b'!'
          | b'$'
          | b'&'
          | b'\''
          | b'('
          | b')'
          | b'*'
          | b'+'
          | b','
          | b';'
          | b'='
          | b'%'
      )
  });
  let quoted_string_table = generate_table(|byte| {
    byte == 0x09
      || byte == 0x20
      || byte == 0x21
      || (0x23..=0x5b).contains(&byte)
      || (0x5d..=0x7e).contains(&byte)
      || byte >= 0x80
  });
  let quoted_pair_table =
    generate_table(|byte| byte == 0x09 || byte == 0x20 || (0x21..=0x7e).contains(&byte) || byte >= 0x80);

  TokenStream::from(quote! {
    pub type StateHandler = fn (parser: &mut Parser, data: &[c_uchar], available: usize);

    #[unsafe(no_mangle)]
    pub type Callback = fn (&mut Parser, usize, usize);

    #(#methods_consts)*
    #(#errors_consts)*
    #(#callbacks_consts)*
    pub const EVENT_END: u8 = 0;
    #(#event_consts)*
    #(#callbacks_bitmask)*
    #(#event_bitmask)*
    #(#states_consts)*
    #parser_field_offsets

    const EVENTS_BUFFER_SIZE: usize = 64 * 1024;
    const EVENT_RANGE_SIZE: usize = 9;
    const EVENT_METADATA_SIZE: usize = 19;
    const EVENT_ERROR_SIZE: usize = 6;
    const EVENT_END_SIZE: usize = 1;
    const EVENT_ERROR_RESERVE: usize = EVENT_ERROR_SIZE + EVENT_END_SIZE;
    // Reserve MESSAGE_COMPLETE, RESET and FINISH, regardless of which events are enabled.
    const EVENT_COMPLETION_SIZE: usize = 3 * EVENT_RANGE_SIZE;
    // Include DATA and BODY before the completion group.
    const EVENT_BODY_COMPLETION_SIZE: usize = 2 * EVENT_RANGE_SIZE + EVENT_COMPLETION_SIZE;
    // Include TRAILERS before the completion group.
    const EVENT_TRAILER_COMPLETION_SIZE: usize = EVENT_RANGE_SIZE + EVENT_COMPLETION_SIZE;

    /// cbindgen:ignore
    static TOKEN_TABLE: [bool; 256] = [#(#token_table),*];

    /// cbindgen:ignore
    static URL_TABLE: [bool; 256] = [#(#url_table),*];

    /// cbindgen:ignore
    static QUOTED_STRING_TABLE: [bool; 256] = [#(#quoted_string_table),*];

    /// cbindgen:ignore
    static QUOTED_PAIR_TABLE: [bool; 256] = [#(#quoted_pair_table),*];
  })
}

/// Generates all parser enums.
fn generate_enums(methods: &[String], errors: &[String], callbacks: &[String], states: &[String]) -> TokenStream {
  let snake_matcher = Regex::new(r"_([a-z])").unwrap();

  let methods_ref = methods;
  let errors_ref = errors;
  let callbacks_ref = callbacks;
  let events_ref: Vec<String> = core::iter::once("END".to_string())
    .chain(
      callbacks_ref
        .iter()
        .map(|x| x.strip_prefix("on_").unwrap_or(x).to_string()),
    )
    .collect();
  let states_ref = states;

  let methods: Vec<_> = methods_ref
    .iter()
    .map(|x| format_ident!("{}", x.replace('-', "_")))
    .collect();

  let errors: Vec<_> = errors_ref.iter().map(|x| format_ident!("{}", x)).collect();

  let callbacks: Vec<_> = callbacks_ref
    .iter()
    .map(|x| {
      let lowercase = x.to_lowercase();

      format_ident!(
        "{}",
        snake_matcher.replace_all(lowercase.as_str(), |captures: &Captures| captures[1].to_uppercase())
      )
    })
    .collect();

  let events: Vec<_> = events_ref
    .iter()
    .map(|x| format_ident!("{}", x.to_uppercase()))
    .collect();

  let states: Vec<_> = states_ref
    .iter()
    .map(|x| format_ident!("{}", x.to_uppercase()))
    .collect();

  let methods_from: Vec<_> = methods_ref
    .iter()
    .enumerate()
    .map(|(x, i)| parse_str::<Arm>(&format!("{} => Ok(Methods::{})", x, i.replace('-', "_"))).unwrap())
    .collect();

  let errors_from: Vec<_> = errors_ref
    .iter()
    .enumerate()
    .map(|(x, i)| parse_str::<Arm>(&format!("{} => Ok(Errors::{})", x, i)).unwrap())
    .collect();

  let callbacks_from: Vec<_> = callbacks
    .iter()
    .enumerate()
    .map(|(x, i)| parse_str::<Arm>(&format!("{} => Ok(Callbacks::{})", x, i)).unwrap())
    .collect();

  let events_from: Vec<_> = events
    .iter()
    .enumerate()
    .map(|(x, i)| parse_str::<Arm>(&format!("{} => Ok(Events::{})", x, i)).unwrap())
    .collect();

  let states_from: Vec<_> = states
    .iter()
    .enumerate()
    .map(|(x, i)| parse_str::<Arm>(&format!("{} => Ok(States::{})", x, i)).unwrap())
    .collect();

  let methods_into: Vec<_> = methods_ref
    .iter()
    .map(|x| parse_str::<Arm>(&format!("Methods::{} => \"{}\"", x.replace('-', "_"), x)).unwrap())
    .collect();

  let errors_into: Vec<_> = errors_ref
    .iter()
    .map(|x| parse_str::<Arm>(&format!("Errors::{} => \"{}\"", x, x)).unwrap())
    .collect();

  let callbacks_into: Vec<_> = callbacks
    .iter()
    .map(|x| parse_str::<Arm>(&format!("Callbacks::{} => \"{}\"", x, x)).unwrap())
    .collect();

  let events_into: Vec<_> = events_ref
    .iter()
    .map(|x| parse_str::<Arm>(&format!("Events::{} => \"{}\"", x.to_uppercase(), x.to_uppercase())).unwrap())
    .collect();

  let states_into: Vec<_> = states
    .iter()
    .map(|x| parse_str::<Arm>(&format!("States::{} => \"{}\"", x, x)).unwrap())
    .collect();

  TokenStream::from(quote! {
    #[repr(u8)]
    #[derive(Copy, Clone, Debug)]
    pub enum Methods {
      #(#methods),*
    }

    #[repr(u8)]
    #[derive(Copy, Clone, Debug)]
    pub enum Errors {
      #(#errors),*
    }

    #[repr(u8)]
    #[derive(Copy, Clone, Debug)]
    pub enum Callbacks {
      #(#callbacks),*
    }

    #[repr(u8)]
    #[derive(Copy, Clone, Debug)]
    pub enum Events {
      #(#events),*
    }

    #[repr(u8)]
    #[derive(Copy, Clone, Debug)]
    pub enum States {
      #(#states),*
    }

    impl TryFrom<u8> for Methods {
      type Error = ();

      fn try_from(value: u8) -> Result<Self, ()> {
        match value {
          #(#methods_from),*,
          _ => Err(())
        }
      }
    }

    impl TryFrom<u8> for Errors {
      type Error = ();

      fn try_from(value: u8) -> Result<Self, ()> {
        match value {
          #(#errors_from),*,
          _ => Err(())
        }
      }
    }

    impl TryFrom<u8> for Callbacks {
      type Error = ();

      fn try_from(value: u8) -> Result<Self, ()> {
        match value {
          #(#callbacks_from),*,
          _ => Err(())
        }
      }
    }

    impl TryFrom<u8> for Events {
      type Error = ();

      fn try_from(value: u8) -> Result<Self, ()> {
        match value {
          #(#events_from),*,
          _ => Err(())
        }
      }
    }

    impl TryFrom<u8> for States {
      type Error = ();

      fn try_from(value: u8) -> Result<Self, ()> {
        match value {
          #(#states_from),*,
          _ => Err(())
        }
      }
    }

    impl From<Methods> for &str {
      fn from(value: Methods) -> Self {
        match value {
          #(#methods_into),*
        }
      }
    }

    impl From<Errors> for &str {
      fn from(value: Errors) -> Self {
        match value {
          #(#errors_into),*
        }
      }
    }

    impl From<Callbacks> for &str {
      fn from(value: Callbacks) -> Self {
        match value {
          #(#callbacks_into),*
        }
      }
    }

    impl From<Events> for &str {
      fn from(value: Events) -> Self {
        match value {
          #(#events_into),*
        }
      }
    }

    impl From<States> for &str {
      fn from(value: States) -> Self {
        match value {
          #(#states_into),*
        }
      }
    }

    impl Methods {
      pub fn as_str(self) -> &'static str {
        self.into()
      }
    }

    impl Errors {
      pub fn as_str(self) -> &'static str {
        self.into()
      }
    }

    impl Callbacks {
      pub fn as_str(self) -> &'static str {
        self.into()
      }
    }

    impl Events {
      pub fn as_str(self) -> &'static str {
        self.into()
      }
    }

    impl States {
      pub fn as_str(self) -> &'static str {
        self.into()
      }
    }
  })
}

fn generate_callbacks(callbacks: &[String]) -> TokenStream {
  let native = native::generate_callbacks(callbacks);
  let wasm = wasm::generate_callbacks(callbacks);

  TokenStream::from_iter([native, wasm])
}

/// Generates the complete parser.
pub fn generate() -> TokenStream {
  let (methods, errors, callbacks, states) = init_constants();

  let constants_code = generate_constants(&methods, &errors, &callbacks, &states);
  let enums_code = generate_enums(&methods, &errors, &callbacks, &states);
  let callbacks_code = generate_callbacks(&callbacks);

  TokenStream::from_iter([constants_code, enums_code, callbacks_code])
}
