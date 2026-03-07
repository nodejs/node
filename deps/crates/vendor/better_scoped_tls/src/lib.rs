//! Better scoped thread local storage.

#[doc(hidden)]
pub extern crate scoped_tls;

/// See [scoped_tls::scoped_thread_local] for actual documentation.
///
/// This is identical as [scoped_tls::scoped_thread_local] on release builds.
#[macro_export]
macro_rules! scoped_tls {
    ($(#[$attrs:meta])* $vis:vis static $name:ident: $ty:ty) => {
        $crate::scoped_tls::scoped_thread_local!(
            static INNER: $ty
        );


        $(#[$attrs])*
        $vis static $name: $crate::ScopedKey<$ty> = $crate::ScopedKey {
            inner: &INNER,
            #[cfg(debug_assertions)]
            module_path: module_path!(),
            #[cfg(debug_assertions)]
            name: stringify!($name),
        };
    };
}

/// Wrapper for [scoped_tls::ScopedKey] with better error messages.
pub struct ScopedKey<T>
where
    T: 'static,
{
    #[doc(hidden)]
    pub inner: &'static scoped_tls::ScopedKey<T>,

    #[cfg(debug_assertions)]
    #[doc(hidden)]
    pub module_path: &'static str,

    #[cfg(debug_assertions)]
    #[doc(hidden)]
    pub name: &'static str,
}

impl<T> ScopedKey<T>
where
    T: 'static,
{
    /// See [scoped_tls::ScopedKey] for actual documentation.
    #[cfg_attr(not(debug_assertions), inline(always))]
    pub fn set<F, R>(&'static self, t: &T, f: F) -> R
    where
        F: FnOnce() -> R,
    {
        self.inner.set(t, f)
    }

    /// See [scoped_tls::ScopedKey] for actual documentation.
    #[track_caller]
    #[cfg_attr(not(debug_assertions), inline(always))]
    pub fn with<F, R>(&'static self, f: F) -> R
    where
        F: FnOnce(&T) -> R,
    {
        #[cfg(debug_assertions)]
        if !self.inner.is_set() {
            // Override panic message
            panic!(
                "You should perform this operation in the closure passed to `set` of {}::{}",
                self.module_path, self.name
            )
        }

        self.inner.with(f)
    }

    /// See [scoped_tls::ScopedKey] for actual documentation.
    #[cfg_attr(not(debug_assertions), inline(always))]
    pub fn is_set(&'static self) -> bool {
        self.inner.is_set()
    }
}

#[cfg(test)]
mod tests {

    scoped_tls!(
        pub static TESTTLS: String
    );

    #[cfg(debug_assertions)]
    #[test]
    #[should_panic = "You should perform this operation in the closure passed to `set` of \
                      better_scoped_tls::tests::TESTTLS"]
    fn panic_on_with() {
        TESTTLS.with(|s| {
            println!("S: {s}");
        })
    }
    #[test]

    fn valid() {
        TESTTLS.set(&String::new(), || {
            TESTTLS.with(|s| {
                assert_eq!(*s, String::new());
            })
        })
    }
}
