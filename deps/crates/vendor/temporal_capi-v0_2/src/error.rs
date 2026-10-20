#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use diplomat_runtime::{DiplomatOption, DiplomatUtf8StrSlice};

    #[diplomat::enum_convert(temporal_rs::error::ErrorKind)]
    pub enum ErrorKind {
        Generic,
        Type,
        Range,
        Syntax,
        Assert,
    }

    // In the future we might turn this into an opaque type with a msg() field
    pub struct TemporalError {
        pub kind: ErrorKind,
        pub msg: DiplomatOption<DiplomatUtf8StrSlice<'static>>,
    }

    impl TemporalError {
        pub(crate) fn range(msg: &'static str) -> Self {
            TemporalError {
                kind: ErrorKind::Range,
                msg: Some(DiplomatUtf8StrSlice::from(msg)).into(),
            }
        }
        pub(crate) fn assert(msg: &'static str) -> Self {
            TemporalError {
                kind: ErrorKind::Assert,
                msg: Some(DiplomatUtf8StrSlice::from(msg)).into(),
            }
        }
    }
}

impl From<temporal_rs::TemporalError> for ffi::TemporalError {
    fn from(other: temporal_rs::TemporalError) -> Self {
        let kind = other.kind().into();
        let msg = other.into_message();
        Self {
            kind,
            msg: Some(msg.into()).into(),
        }
    }
}
