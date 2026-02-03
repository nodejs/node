//! Methods for types and navigating lifetimes within methods.

use std::collections::BTreeSet;
use std::ops::Deref;

use super::{
    Attrs, Docs, Ident, IdentBuf, InputOnly, OutType, OutputOnly, SelfType, TraitPath, Type,
    TypeContext,
};

use super::lifetimes::{Lifetime, LifetimeEnv, Lifetimes, MaybeStatic};

use borrowing_field::BorrowingFieldVisitor;
use borrowing_param::BorrowingParamVisitor;

pub mod borrowing_field;
pub mod borrowing_param;

/// A method exposed to Diplomat.
/// Used for representing both free functions ([`crate::ast::Function`]) and struct methods ([`crate::ast::Method`]).
/// The only difference between a free function and a struct method in this struct is that [`Self::param_self`] will always be `None` for a free function.
#[derive(Debug)]
#[non_exhaustive]
pub struct Method {
    /// Documentation specified on the method
    pub docs: Docs,
    /// The name of the method as initially declared.
    pub name: IdentBuf,
    /// The name of the generated `extern "C"` function
    pub abi_name: IdentBuf,
    /// The lifetimes introduced in this method and surrounding impl block.
    pub lifetime_env: LifetimeEnv,

    /// An &self, &mut self, or Self parameter
    pub param_self: Option<ParamSelf>,
    /// The parameters of the method
    pub params: Vec<Param>,
    /// The output type, including whether it returns a Result/Option/Writeable/etc
    pub output: ReturnType,
    /// Resolved (and inherited) diplomat::attr attributes on this method
    pub attrs: Attrs,
}

pub trait CallbackInstantiationFunctionality {
    #[allow(clippy::result_unit_err)]
    fn get_inputs(&self) -> Result<&[CallbackParam], ()>; // the types of the parameters
    #[allow(clippy::result_unit_err)]
    fn get_output_type(&self) -> Result<&ReturnType<InputOnly>, ()>;
}

#[derive(Debug, Clone)]
#[non_exhaustive]
// Note: we do not support borrowing across callbacks
pub struct Callback {
    pub param_self: Option<TraitParamSelf>, // this is None for callbacks as method arguments
    pub params: Vec<CallbackParam>,
    pub output: Box<ReturnType<InputOnly>>, // this will be used in Rust (note: can technically be a callback, or void)
    pub name: Option<IdentBuf>,
    pub attrs: Option<Attrs>,
    pub docs: Option<Docs>,
}

// uninstantiatable; represents no callback allowed
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum NoCallback {}

impl CallbackInstantiationFunctionality for Callback {
    fn get_inputs(&self) -> Result<&[CallbackParam], ()> {
        Ok(&self.params)
    }
    fn get_output_type(&self) -> Result<&ReturnType<InputOnly>, ()> {
        Ok(&self.output)
    }
}

impl CallbackInstantiationFunctionality for NoCallback {
    fn get_inputs(&self) -> Result<&[CallbackParam], ()> {
        Err(())
    }
    fn get_output_type(&self) -> Result<&ReturnType<InputOnly>, ()> {
        Err(())
    }
}

/// Type that the method returns.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum SuccessType<P: super::TyPosition = OutputOnly> {
    /// Conceptually returns a string, which gets written to the `write: DiplomatWrite` argument
    Write,
    /// A Diplomat type. Some types can be outputs, but not inputs, which is expressed by the `OutType` parameter.
    OutType(Type<P>),
    /// A `()` type in Rust.
    Unit,
}

/// Whether or not the method returns a value or a result.
#[derive(Debug, Clone)]
#[allow(clippy::exhaustive_enums)] // this only exists for fallible/infallible, breaking changes for more complex returns are ok
pub enum ReturnType<P: super::TyPosition = OutputOnly> {
    Infallible(SuccessType<P>),
    Fallible(SuccessType<P>, Option<Type<P>>),
    Nullable(SuccessType<P>),
}

/// The `self` parameter of a method.
#[derive(Debug)]
#[non_exhaustive]
pub struct ParamSelf {
    pub ty: SelfType,
    pub attrs: Attrs,
}

#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct TraitParamSelf {
    pub trait_path: TraitPath,
}

/// A parameter in a method.
#[derive(Debug)]
#[non_exhaustive]
pub struct Param {
    pub name: IdentBuf,
    pub ty: Type<InputOnly>,
    pub attrs: Attrs,
}

/// A parameter in a callback
/// No name, since all we get is the callback type signature
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct CallbackParam {
    pub ty: Type<OutputOnly>,
    pub name: Option<IdentBuf>,
}

impl SuccessType {
    /// Returns whether the variant is `Write`.
    pub fn is_write(&self) -> bool {
        matches!(self, SuccessType::Write)
    }

    /// Returns whether the variant is `Unit`.
    pub fn is_unit(&self) -> bool {
        matches!(self, SuccessType::Unit)
    }

    pub fn as_type(&self) -> Option<&OutType> {
        match self {
            SuccessType::OutType(ty) => Some(ty),
            _ => None,
        }
    }
}

impl Deref for ReturnType {
    type Target = SuccessType;

    fn deref(&self) -> &Self::Target {
        match self {
            ReturnType::Infallible(ret) | ReturnType::Fallible(ret, _) | Self::Nullable(ret) => ret,
        }
    }
}

impl ReturnType {
    /// Returns `true` if the FFI function returns `void`. Not that this is different from `is_unit`,
    /// which will be true for `DiplomatResult<(), E>` and false for infallible write.
    pub fn is_ffi_unit(&self) -> bool {
        matches!(
            self,
            ReturnType::Infallible(SuccessType::Unit | SuccessType::Write)
        )
    }

    /// The "main" return type of this function: the Ok, Some, or regular type
    pub fn success_type(&self) -> &SuccessType {
        match &self {
            Self::Infallible(s) => s,
            Self::Fallible(s, _) => s,
            Self::Nullable(s) => s,
        }
    }

    /// Get the list of method lifetimes actually used by the method return type
    ///
    /// Most input lifetimes aren't actually used. An input lifetime is generated
    /// for each borrowing parameter but is only important if we use it in the return.
    pub fn used_method_lifetimes(&self) -> BTreeSet<Lifetime> {
        let mut set = BTreeSet::new();

        let mut add_to_set = |ty: &OutType| {
            for lt in ty.lifetimes() {
                if let MaybeStatic::NonStatic(lt) = lt {
                    set.insert(lt);
                }
            }
        };

        match self {
            ReturnType::Infallible(SuccessType::OutType(ref ty))
            | ReturnType::Nullable(SuccessType::OutType(ref ty)) => add_to_set(ty),
            ReturnType::Fallible(ref ok, ref err) => {
                if let SuccessType::OutType(ref ty) = ok {
                    add_to_set(ty)
                }
                if let Some(ref ty) = err {
                    add_to_set(ty)
                }
            }
            _ => (),
        }

        set
    }

    pub fn with_contained_types(&self, mut f: impl FnMut(&OutType)) {
        match self {
            Self::Infallible(SuccessType::OutType(o))
            | Self::Nullable(SuccessType::OutType(o))
            | Self::Fallible(SuccessType::OutType(o), None) => f(o),
            Self::Fallible(SuccessType::OutType(o), Some(o2)) => {
                f(o);
                f(o2)
            }
            Self::Fallible(_, Some(o)) => f(o),
            _ => (),
        }
    }
}

impl ParamSelf {
    pub(super) fn new(ty: SelfType, attrs: Attrs) -> Self {
        Self { ty, attrs }
    }

    /// Return the number of fields and leaves that will show up in the [`BorrowingFieldVisitor`].
    ///
    /// This method is used to calculate how much space to allocate upfront.
    fn field_leaf_lifetime_counts(&self, tcx: &TypeContext) -> (usize, usize) {
        match self.ty {
            SelfType::Opaque(_) => (1, 1),
            SelfType::Struct(ref ty) => ty.resolve(tcx).fields.iter().fold((1, 0), |acc, field| {
                let inner = field.ty.field_leaf_lifetime_counts(tcx);
                (acc.0 + inner.0, acc.1 + inner.1)
            }),
            SelfType::Enum(_) => (0, 0),
        }
    }
}

impl TraitParamSelf {
    pub(super) fn new(trait_path: TraitPath) -> Self {
        Self { trait_path }
    }
}

impl Param {
    pub(super) fn new(name: IdentBuf, ty: Type<InputOnly>, attrs: Attrs) -> Self {
        Self { name, ty, attrs }
    }
}

impl Method {
    /// Returns a fresh [`Lifetimes`] corresponding to `self`.
    pub fn method_lifetimes(&self) -> Lifetimes {
        self.lifetime_env.lifetimes()
    }

    /// Returns a new [`BorrowingParamVisitor`], which can *shallowly* link output lifetimes
    /// to the parameters they borrow from.
    ///
    /// This is useful for backends which wish to have lifetime codegen for methods only handle the local
    /// method lifetime, and delegate to generated code on structs for handling the internals of struct lifetimes.
    ///
    /// `force_include_slices` is right now *just* for the JS backend.
    /// Because the JS backend requires us to know information about the allocation of each slice,
    /// then we need to grab that information in the [`BorrowingParamVisitor`].
    /// See [`BorrowingParamVisitor::new`] for more.
    pub fn borrowing_param_visitor<'tcx>(
        &'tcx self,
        tcx: &'tcx TypeContext,
        force_include_slices: bool,
    ) -> BorrowingParamVisitor<'tcx> {
        BorrowingParamVisitor::new(self, tcx, force_include_slices)
    }

    /// Returns a new [`BorrowingFieldVisitor`], which allocates memory to
    /// efficiently represent all fields (and their paths!) of the inputs that
    /// have a lifetime.
    ///
    /// This is useful for backends which wish to "splat out" lifetime edge codegen for methods,
    /// linking each borrowed input param/field (however deep it may be in a struct) to a borrowed output param/field.
    ///
    /// ```ignore
    /// # use std::collections::BTreeMap;
    /// let visitor = method.borrowing_field_visitor(&tcx, "this".ck().unwrap());
    /// let mut map = BTreeMap::new();
    /// visitor.visit_borrowing_fields(|lifetime, field| {
    ///     map.entry(lifetime).or_default().push(field);
    /// })
    /// ```
    pub fn borrowing_field_visitor<'m>(
        &'m self,
        tcx: &'m TypeContext,
        self_name: &'m Ident,
    ) -> BorrowingFieldVisitor<'m> {
        BorrowingFieldVisitor::new(self, tcx, self_name)
    }
}
