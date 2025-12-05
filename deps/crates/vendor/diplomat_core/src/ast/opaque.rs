use serde::Serialize;

use super::docs::Docs;
use super::{Attrs, Ident, LifetimeEnv, Method, Mutability};

/// A type annotated with [`diplomat::opaque`] whose fields/variants are not visible.
/// Opaque types cannot be passed by-value across the FFI boundary, so they
/// must be boxed or passed as references.
#[derive(Clone, Serialize, Debug, Hash, PartialEq, Eq)]
#[non_exhaustive]
pub struct OpaqueType {
    pub name: Ident,
    pub docs: Docs,
    pub lifetimes: LifetimeEnv,
    pub methods: Vec<Method>,
    pub mutability: Mutability,
    pub attrs: Attrs,
    /// The ABI name of the generated destructor
    pub dtor_abi_name: Ident,
}

impl OpaqueType {
    /// Extract a [`OpaqueType`] metadata value from an AST node representing a struct.
    pub fn new_struct(
        strct: &syn::ItemStruct,
        mutability: Mutability,
        parent_attrs: &Attrs,
    ) -> Self {
        let mut attrs = parent_attrs.clone();
        attrs.add_attrs(&strct.attrs);
        let name = Ident::from(&strct.ident);
        OpaqueType {
            dtor_abi_name: Self::dtor_abi_name(&name, &attrs),
            name,
            docs: Docs::from_attrs(&strct.attrs),
            lifetimes: LifetimeEnv::from_struct_item(strct, &[]),
            methods: vec![],
            mutability,
            attrs,
        }
    }

    /// Extract a [`OpaqueType`] metadata value from an AST node representing an enum.
    pub fn new_enum(enm: &syn::ItemEnum, mutability: Mutability, parent_attrs: &Attrs) -> Self {
        let mut attrs = parent_attrs.clone();
        attrs.add_attrs(&enm.attrs);
        let name = Ident::from(&enm.ident);
        OpaqueType {
            dtor_abi_name: Self::dtor_abi_name(&name, &attrs),
            name,
            docs: Docs::from_attrs(&enm.attrs),
            lifetimes: LifetimeEnv::from_enum_item(enm, &[]),
            methods: vec![],
            mutability,
            attrs,
        }
    }

    fn dtor_abi_name(name: &Ident, attrs: &Attrs) -> Ident {
        let dtor_abi_name = format!("{name}_destroy");
        let dtor_abi_name = String::from(attrs.abi_rename.apply(dtor_abi_name.into()));
        Ident::from(dtor_abi_name)
    }
}
