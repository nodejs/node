#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum TimeZoneProviderError {
    InstantOutOfRange,
    Assert(&'static str),
    Range(&'static str),
}
