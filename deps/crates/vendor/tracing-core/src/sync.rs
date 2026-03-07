pub(crate) use crate::spin::MutexGuard;

/// This wraps `spin::Mutex` to return a `Result`, so that it can be
/// used with code written against `std::sync::Mutex`.
///
/// Since `spin::Mutex` doesn't support poisoning, the `Result` returned
/// by `lock` will always be `Ok`.
#[derive(Debug, Default)]
pub(crate) struct Mutex<T> {
    inner: crate::spin::Mutex<T>,
}

impl<T> Mutex<T> {
    // pub(crate) fn new(data: T) -> Self {
    //     Self {
    //         inner: crate::spin::Mutex::new(data),
    //     }
    // }

    pub(crate) fn lock(&self) -> Result<MutexGuard<'_, T>, ()> {
        Ok(self.inner.lock())
    }
}
