extern crate proc_macro;

use proc_macro2::TokenStream;
use swc_macros_common::prelude::*;
use syn::{
    fold::Fold,
    parse::{Parse, Parser},
    token::Token,
    *,
};

///
///
/// # Example
///
/// ```ignore
/// impl MacroNode for ParamOrTsParamProp {
///     fn emit(&mut self, emitter: &mut Macro) -> Result {
///         match self {
///             ParamOrTsParamProp::Param(n) => emit!(n),
///             ParamOrTsParamProp::TsParamProp(n) => emit!(n),
///         }
///     }
/// }
/// ```
///
///
/// ## `emit!()`.
///
/// `emit!()` macro in `#[node_impl]` functions are special.
///
/// Those are replaced with `emit_with` and `adjust_span` methods, depending on
/// the context.
#[proc_macro_attribute]
pub fn node_impl(
    _attr: proc_macro::TokenStream,
    item: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let item: ItemImpl = syn::parse(item).expect("failed to parse input as an item");

    let mut output = TokenStream::new();

    for i in item.items {
        match i {
            ImplItem::Fn(i) => {
                let item = expand_node_impl_method(&item.self_ty, i);

                output.extend(item.to_token_stream());
            }

            _ => {
                panic!("Unexpected item: {i:?}");
            }
        }
    }

    output.into()
}

/// Returns `(emitter_method, adjuster_method)`
fn expand_node_impl_method(node_type: &Type, src: ImplItemFn) -> ItemImpl {
    let emit_block = ReplaceEmit { emit: true }.fold_block(src.block.clone());

    parse_quote!(
        impl crate::Node for #node_type {
            fn emit_with<W, S>(&self, emitter: &mut crate::Emitter<'_, W, S>) -> crate::Result
            where
                W: crate::text_writer::WriteJs,
                S: swc_common::SourceMapper + swc_ecma_ast::SourceMapperExt,
            {
                #emit_block
            }

        }
    )
}

struct ReplaceEmit {
    emit: bool,
}

impl Fold for ReplaceEmit {
    fn fold_macro(&mut self, mut i: Macro) -> Macro {
        let name = i.path.clone().into_token_stream().to_string();

        if "emit" == &*name {
            let args: Punctuated<Expr, Token![,]> = parse_args(i.tokens);
            let args: TokenStream = args
                .into_pairs()
                .flat_map(|arg| arg.into_token_stream())
                .collect();

            let is_emit = self.emit;
            i = parse_quote!(emit_node_inner!(emitter, #is_emit, #args));
        }

        i
    }
}

fn parse_args<T, P>(tokens: TokenStream) -> Punctuated<T, P>
where
    T: Parse,
    P: Parse + Token,
{
    let parser = Punctuated::parse_separated_nonempty;
    parser.parse2(tokens).expect("failed parse args")
}
