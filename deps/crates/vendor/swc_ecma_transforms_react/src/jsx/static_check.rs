use swc_ecma_ast::*;

/// We want to use React.createElement, even in the case of
/// jsx, for <div {...props} key={key} /> to distinguish it
/// from <div key={key} {...props} />. This is an intermediary
/// step while we deprecate key spread from props. Afterwards,
/// we will stop using createElement in the transform.
pub(super) fn should_use_create_element(attrs: &[JSXAttrOrSpread]) -> bool {
    let mut seen_prop_spread = false;
    for attr in attrs {
        if seen_prop_spread
            && match attr {
                JSXAttrOrSpread::JSXAttr(attr) => match &attr.name {
                    JSXAttrName::Ident(i) => i.sym == "key",
                    JSXAttrName::JSXNamespacedName(_) => false,
                    #[cfg(swc_ast_unknown)]
                    _ => panic!("unable to access unknown nodes"),
                },
                _ => false,
            }
        {
            return true;
        }

        if let JSXAttrOrSpread::SpreadElement(_) = attr {
            seen_prop_spread = true;
        }
    }

    false
}
