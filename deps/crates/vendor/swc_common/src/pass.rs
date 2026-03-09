//! This module reexports items from `swc_visit` with some swc-specific traits.

use std::borrow::Cow;

pub use swc_visit::*;

/// A named compiler pass.
pub trait CompilerPass {
    ///
    /// - name should follow hyphen-case.
    /// - an implementation should return same name
    fn name(&self) -> Cow<'static, str>;
}

impl<V> CompilerPass for Repeat<V>
where
    V: CompilerPass + Repeated,
{
    fn name(&self) -> Cow<'static, str> {
        Cow::Owned(format!("repeat({})", self.pass.name()))
    }
}

macro_rules! impl_compiler_pass_for_tuple {
    (
        [$idx:tt, $name:ident], $([$idx_rest:tt, $name_rest:ident]),*
    ) => {
        impl<$name, $($name_rest),*> CompilerPass for ($name, $($name_rest),*)
        where
            $name: CompilerPass,
            $($name_rest: CompilerPass),*
        {
            #[inline]
            fn name(&self) -> Cow<'static, str> {
                use std::fmt::Write;

                let mut name = self.$idx.name().to_string();
                $(
                    write!(name, " / {}", self.$idx_rest.name()).unwrap();
                )*
                Cow::Owned(name)
            }
        }
    };
}

impl_compiler_pass_for_tuple!([0, A], [1, B]);
impl_compiler_pass_for_tuple!([0, A], [1, B], [2, C]);
impl_compiler_pass_for_tuple!([0, A], [1, B], [2, C], [3, D]);
impl_compiler_pass_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E]);
impl_compiler_pass_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F]);
impl_compiler_pass_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F], [6, G]);
impl_compiler_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H]
);
impl_compiler_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I]
);
impl_compiler_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J]
);
impl_compiler_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K]
);
impl_compiler_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K],
    [11, L]
);
impl_compiler_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K],
    [11, L],
    [12, M]
);
