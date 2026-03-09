use swc_macros_common::prelude::*;
use syn::{
    parse::{Parse, ParseStream},
    *,
};

#[derive(Clone)]
pub struct Args {
    pub ty: Literal,
}

impl Parse for Args {
    fn parse(i: ParseStream<'_>) -> syn::Result<Self> {
        Ok(Args { ty: i.parse()? })
    }
}

pub fn expand_struct(args: Args, i: DeriveInput) -> Vec<ItemImpl> {
    let mut items = Vec::new();
    let generics = i.generics.clone();
    // let item_ident = Ident::new("Item", i.ident.span());

    {
        let ty = &i.ident;
        let type_str = &args.ty;
        let item: ItemImpl = parse_quote!(
            impl ::swc_common::AstNode for #ty {
                const TYPE: &'static str = #type_str;
            }
        );
        items.push(item.with_generics(generics));
    }

    // let ident = i.ident.clone();
    // let cloned = i.clone();

    // items.push({
    //     let (fields, item_data) = match i.data {
    //         Data::Struct(DataStruct {
    //             struct_token,
    //             semi_token,
    //             fields: Fields::Named(FieldsNamed { brace_token, named }),
    //         }) => {
    //             let fields: Punctuated<_, token::Comma> = named
    //                 .clone()
    //                 .into_iter()
    //                 .map(|field| FieldValue {
    //                     member: Member::Named(field.ident.clone().unwrap()),
    //                     expr: Quote::new_call_site()
    //                         .quote_with(smart_quote!(
    //                             Vars {
    //                                 field: &field.ident
    //                             },
    //                             { node.node.field }
    //                         ))
    //                         .parse(),

    //                     attrs: field
    //                         .attrs
    //                         .into_iter()
    //                         .filter(|attr| is_attr_name(attr, "cfg"))
    //                         .collect(),
    //                     colon_token: Some(call_site()),
    //                 })
    //                 .collect();

    //             let item_data = Data::Struct(DataStruct {
    //                 struct_token,
    //                 semi_token,
    //                 fields: Fields::Named(FieldsNamed {
    //                     brace_token,
    //                     named: named
    //                         .into_pairs()
    //                         .map(|pair| {
    //                             let handle = |v: Field| Field {
    //                                 vis: Visibility::Inherited,
    //                                 attrs: v
    //                                     .attrs
    //                                     .into_iter()
    //                                     .filter(|attr| {
    //                                         is_attr_name(attr, "serde") ||
    // is_attr_name(attr, "cfg")                                     })
    //                                     .collect(),
    //                                 ..v
    //                             };

    //                             match pair {
    //                                 Pair::End(v) => Pair::End(handle(v)),
    //                                 Pair::Punctuated(v, p) =>
    // Pair::Punctuated(handle(v), p),                             }
    //                         })
    //                         .collect(),
    //                 }),
    //             });

    //             (fields, item_data)
    //         }
    //         _ => unreachable!("enum / tuple struct / union with
    // #[ast_node(\"Foo\")]"),     };

    //     let convert_item_to_self =
    // Quote::new_call_site().quote_with(smart_quote!(         Vars {
    //             fields,
    //             Type: &ident
    //         },
    //         { Type { fields } }
    //     ));

    //     let body = Quote::new_call_site().quote_with(smart_quote!(
    //         Vars {
    //             convert_item_to_self
    //         },
    //         {
    //             let node =
    // ::swc_common::serializer::Node::<Item>::deserialize(deserializer)?;

    //             if node.ty != <Self as ::swc_common::AstNode>::TYPE {
    //                 return Err(D::Error::unknown_variant(
    //                     &node.ty,
    //                     &[<Self as ::swc_common::AstNode>::TYPE],
    //                 ));
    //             }

    //             Ok(convert_item_to_self)
    //         }
    //     ));

    //     let item = DeriveInput {
    //         vis: Visibility::Inherited,
    //         ident: item_ident,
    //         attrs: Vec::new(),
    //         data: item_data,
    //         ..cloned
    //     };
    //     Quote::new_call_site()
    //         .quote_with(smart_quote!(
    //             Vars {
    //                 // A new item which implements Deserialize
    //                 item,
    //                 Type: ident,
    //                 body
    //             },
    //             {
    //                 impl<'de> ::serde::Deserialize<'de> for Type {
    //                     fn deserialize<D>(deserializer: D) -> Result<Self,
    // D::Error>                     where
    //                         D: ::serde::Deserializer<'de>,
    //                     {
    //                         use ::serde::de::Error;
    //                         #[derive(::serde::Deserialize)]
    //                         #[serde(rename_all = "camelCase")]
    //                         ite
    //                         body
    //                     }
    //                 }
    //             }
    //         ))
    //         .parse::<ItemImpl>()
    //         .with_generics(generics)
    // });

    items
}
