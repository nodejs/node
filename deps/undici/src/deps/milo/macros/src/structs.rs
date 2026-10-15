use syn::parse::{Parse, ParseStream};
use syn::{Expr, Ident, Result, Token};

/// An identifier associated to a message, typically a string - An example of
/// this is used in `fail!`.
pub struct FailureRequest {
  pub error: Ident,
  pub message: Expr,
}

/// An identifier associated to an expression - An example of this is used in
/// `event_with_range!`.
pub struct EventRequest {
  pub identifier: Ident,
  pub offset: Option<Expr>,
  pub length: Option<Expr>,
}

impl Parse for FailureRequest {
  // Parses a failure definition
  fn parse(input: ParseStream) -> Result<Self> {
    // Get the code
    let error = input.parse()?;

    // Discard the comma
    input.parse::<Token![,]>()?;

    // Get the message
    let message = input.parse()?;

    Ok(FailureRequest { error, message })
  }
}

impl Parse for EventRequest {
  // Parses a identifier and its optional expression
  fn parse(input: ParseStream) -> Result<Self> {
    let identifier = input.parse()?;
    let mut offset = None;
    let mut length = None;

    // If there is more input
    if !input.is_empty() {
      // Discard the comma
      input.parse::<Token![,]>()?;

      // Parse the expression
      offset = Some(input.parse::<Expr>()?);

      if !input.is_empty() {
        // Discard the comma
        input.parse::<Token![,]>()?;

        // Parse the expression
        length = Some(input.parse::<Expr>()?);
      }
    }

    Ok(EventRequest {
      identifier,
      offset,
      length,
    })
  }
}
