use core::marker::PhantomData;
#[cfg(feature = "zoneinfo64")]
use timezone_provider::zoneinfo64::ZoneInfo64TzdbProvider;

/// We use a macro to avoid dynamic dispatch: a given codebase will
/// typically only be compiled with a single provider.
macro_rules! with_provider(
    ($provider:ident, |$p:ident| $code:expr) => {
        match $provider.0 {
            #[cfg(feature = "zoneinfo64")]
            crate::provider::ProviderInner::ZoneInfo64(ref zi) => {
                let $p = zi;
                $code
            }
            #[cfg(feature = "compiled_data")]
            crate::provider::ProviderInner::Compiled => {
                let $p = &*temporal_rs::provider::COMPILED_TZ_PROVIDER;
                $code
            }
            crate::provider::ProviderInner::Empty(..) => {
                return Err(TemporalError::assert("Failed to load timezone info"));
                // This ensures that type inference still works with --no-default-features
                #[allow(dead_code)] {
                    let $p = &timezone_provider::provider::NeverProvider::default();
                    $code
                }
            }
        }
    };
);

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use super::ProviderInner;
    use alloc::boxed::Box;

    use core::marker::PhantomData;

    #[cfg(feature = "zoneinfo64")]
    use zoneinfo64::ZoneInfo64;

    /// A time zone data provider
    #[diplomat::opaque]
    pub struct Provider<'a>(pub(crate) ProviderInner<'a>);

    impl<'a> Provider<'a> {
        #[cfg(feature = "zoneinfo64")]
        /// Construct a provider backed by a zoneinfo64.res file
        ///
        /// This failing to construct is not a Temporal error, so it just returns ()
        #[allow(
            clippy::result_unit_err,
            reason = "Diplomat distinguishes between Result and Option"
        )]
        pub fn new_zoneinfo64(data: &'a [u32]) -> Result<Box<Provider<'a>>, ()> {
            let zi64 = ZoneInfo64::try_from_u32s(data).map_err(|_| ())?;
            let data = super::ZoneInfo64TzdbProvider::new(zi64);

            Ok(Box::new(Self(ProviderInner::ZoneInfo64(data))))
        }

        #[cfg(feature = "compiled_data")]
        pub fn new_compiled() -> Box<Self> {
            Box::new(Self(ProviderInner::Compiled))
        }

        /// For internal use
        #[cfg(feature = "compiled_data")]
        pub(crate) fn compiled() -> Self {
            Self(ProviderInner::Compiled)
        }

        /// Fallback type in case construction does not work.
        pub fn empty() -> Box<Self> {
            Box::new(Self(ProviderInner::Empty(PhantomData)))
        }
    }
}

pub(crate) enum ProviderInner<'a> {
    #[cfg(feature = "zoneinfo64")]
    ZoneInfo64(ZoneInfo64TzdbProvider<'a>),

    #[cfg(feature = "compiled_data")]
    Compiled,

    /// This avoids unused lifetime errors when the feature is disabled
    Empty(PhantomData<&'a u8>),
}
