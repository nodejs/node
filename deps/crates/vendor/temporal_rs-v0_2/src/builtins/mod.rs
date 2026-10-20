//! The builtins module contains the main implementation of the Temporal builtins

#[cfg(feature = "compiled_data")]
pub mod compiled;
pub mod core;

pub use core::*;

#[cfg(feature = "compiled_data")]
use std::sync::LazyLock;
#[cfg(feature = "compiled_data")]
use timezone_provider::tzif::CompiledTzdbProvider;
#[cfg(all(test, feature = "compiled_data"))]
use timezone_provider::tzif::FsTzdbProvider;

#[cfg(feature = "compiled_data")]
pub static TZ_PROVIDER: LazyLock<CompiledTzdbProvider> =
    LazyLock::new(CompiledTzdbProvider::default);

#[cfg(all(test, feature = "compiled_data"))]
pub(crate) static FS_TZ_PROVIDER: LazyLock<FsTzdbProvider> = LazyLock::new(FsTzdbProvider::default);

#[cfg(test)]
mod tests {
    use super::{Instant, PlainDate, PlainDateTime};
    #[test]
    fn builtins_from_str_10_digit_fractions() {
        // Failure case with 10 digits
        let test = "2020-01-01T00:00:00.1234567890Z";
        let result = Instant::from_utf8(test.as_bytes());
        assert!(result.is_err(), "Instant fraction should be invalid");
        let result = PlainDate::from_utf8(test.as_bytes());
        assert!(result.is_err(), "PlainDate fraction should be invalid");
        let result = PlainDateTime::from_utf8(test.as_bytes());
        assert!(result.is_err(), "PlainDateTime fraction should be invalid");
    }

    #[test]
    fn instant_based_10_digit_offset() {
        let test = "2020-01-01T00:00:00.123456789+02:30:00.1234567890[UTC]";
        let result = Instant::from_utf8(test.as_bytes());
        assert!(result.is_err(), "Instant should be invalid");
    }

    #[test]
    fn instant_based_9_digit_offset() {
        let test = "2020-01-01T00:00:00.123456789+02:30:00.123456789[UTC]";
        let result = Instant::from_utf8(test.as_bytes());
        assert!(result.is_ok(), "Instant should be valid");
    }

    #[test]
    fn builtin_from_str_9_digit_fractions() {
        // Success case with 9 digits
        let test = "2020-01-01T00:00:00.123456789Z";
        let result = Instant::from_utf8(test.as_bytes());
        assert!(result.is_ok(), "Instant fraction should be valid");
        let test = "2020-01-01T00:00:00.123456789";
        let result = PlainDate::from_utf8(test.as_bytes());
        assert!(result.is_ok(), "PlainDate fraction should be valid");
        let result = PlainDateTime::from_utf8(test.as_bytes());
        assert!(result.is_ok(), "PlainDateTime fraction should be valid");
    }
}
