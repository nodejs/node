pub(crate) mod decode;
pub(crate) mod encode;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum EnumType {
    Unit,
    One,
    Struct,
}

pub(crate) fn is_unknown(attrs: &[syn::Attribute]) -> bool {
    attrs
        .iter()
        .filter(|attr| attr.path().is_ident("encoding"))
        .any(|attr| {
            let mut is_unknown = false;
            attr.parse_nested_meta(|meta| {
                is_unknown |= meta.path.is_ident("unknown");
                Ok(())
            })
            .unwrap();
            is_unknown
        })
}

fn is_with(attrs: &[syn::Attribute]) -> Option<syn::Path> {
    attrs
        .iter()
        .filter(|attr| attr.path().is_ident("encoding"))
        .find_map(|attr| {
            let mut with_type = None;
            attr.parse_nested_meta(|meta| {
                if meta.path.is_ident("with") {
                    let val = meta.value()?;
                    let val: syn::LitStr = val.parse()?;
                    let val: syn::Path = val.parse()?;
                    with_type = Some(val);
                }

                Ok(())
            })
            .ok()?;
            with_type
        })
}

fn is_ignore(attrs: &[syn::Attribute]) -> bool {
    attrs
        .iter()
        .filter(|attr| attr.path().is_ident("encoding"))
        .any(|attr| {
            let mut has_ignore = false;
            let _ = attr.parse_nested_meta(|meta| {
                has_ignore |= meta.path.is_ident("ignore");
                Ok(())
            });
            has_ignore
        })
}
