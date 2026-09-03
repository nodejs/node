use core::fmt;
use core::mem::ManuallyDrop;

#[repr(C)]
union DiplomatResultValue<T, E> {
    ok: ManuallyDrop<T>,
    err: ManuallyDrop<E>,
}

/// A [`Result`]-like type that can be passed across the FFI boundary
/// as a value. Used internally to return [`Result`]s and [`Option`]s
/// from functions.
#[repr(C)]
pub struct DiplomatResult<T, E> {
    value: DiplomatResultValue<T, E>,
    pub is_ok: bool,
}

/// A type to represent Option<T> over FFI.
///
/// Used internally to handle `Option<T>` arguments and return types, and needs to be
/// used explicitly for optional struct fields.
pub type DiplomatOption<T> = DiplomatResult<T, ()>;

impl<T, E> DiplomatResult<T, E> {
    pub fn as_ref(&self) -> Result<&T, &E> {
        // Safety: we're only accessing the union variants when the flag is correct
        unsafe {
            if self.is_ok {
                Ok(&self.value.ok)
            } else {
                Err(&self.value.err)
            }
        }
    }
}

impl<T> DiplomatOption<T> {
    /// Helper for converting into an Option to avoid trait ambiguity errors with Into
    #[inline]
    pub fn into_option(self) -> Option<T> {
        self.into()
    }

    /// Helper for converting into an Option with the inner type converted
    #[inline]
    pub fn into_converted_option<U: From<T>>(self) -> Option<U> {
        self.into_option().map(Into::into)
    }
}

impl<T: Clone, E: Clone> Clone for DiplomatResult<T, E> {
    fn clone(&self) -> Self {
        unsafe {
            if self.is_ok {
                Ok((*self.value.ok).clone()).into()
            } else {
                Err((*self.value.err).clone()).into()
            }
        }
    }
}

impl<T, E> Drop for DiplomatResult<T, E> {
    fn drop(&mut self) {
        unsafe {
            if self.is_ok {
                let _ = ManuallyDrop::take(&mut self.value.ok);
            } else {
                let _ = ManuallyDrop::take(&mut self.value.err);
            }
        }
    }
}

impl<T, E> From<Result<T, E>> for DiplomatResult<T, E> {
    fn from(result: Result<T, E>) -> Self {
        match result {
            Result::Ok(ok) => DiplomatResult {
                value: DiplomatResultValue {
                    ok: ManuallyDrop::new(ok),
                },
                is_ok: true,
            },

            Result::Err(err) => DiplomatResult {
                value: DiplomatResultValue {
                    err: ManuallyDrop::new(err),
                },
                is_ok: false,
            },
        }
    }
}

impl<T> From<Option<T>> for DiplomatOption<T> {
    fn from(option: Option<T>) -> Self {
        option.ok_or(()).into()
    }
}

impl<T> From<DiplomatOption<T>> for Option<T> {
    fn from(result: DiplomatOption<T>) -> Self {
        Result::<T, ()>::from(result).ok()
    }
}

impl<T, E> From<DiplomatResult<T, E>> for Result<T, E> {
    fn from(mut result: DiplomatResult<T, E>) -> Result<T, E> {
        unsafe {
            if result.is_ok {
                Ok(ManuallyDrop::take(&mut result.value.ok))
            } else {
                Err(ManuallyDrop::take(&mut result.value.err))
            }
        }
    }
}

impl<T: fmt::Debug, E: fmt::Debug> fmt::Debug for DiplomatResult<T, E> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_ref().fmt(f)
    }
}
