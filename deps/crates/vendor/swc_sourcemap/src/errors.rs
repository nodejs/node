use std::error;
use std::fmt;
use std::io;
use std::str;
use std::string;

/// Represents results from this library
pub type Result<T> = std::result::Result<T, Error>;

/// Represents different failure cases
#[derive(Debug)]
pub enum Error {
    /// a std::io error
    Io(io::Error),
    #[cfg(feature = "ram_bundle")]
    /// a scroll error
    Scroll(scroll::Error),
    /// a std::str::Utf8Error
    Utf8(str::Utf8Error),
    /// a JSON parsing related failure
    BadJson(serde_json::Error),
    /// a VLQ string was malformed and data was left over
    VlqLeftover,
    /// a VLQ string was empty and no values could be decoded.
    VlqNoValues,
    /// Overflow in Vlq handling
    VlqOverflow,
    /// a mapping segment had an unsupported size
    BadSegmentSize(u32),
    /// a reference to a non existing source was encountered
    BadSourceReference(u32),
    /// a reference to a non existing name was encountered
    BadNameReference(u32),
    /// Indicates that an incompatible sourcemap format was encountered
    IncompatibleSourceMap,
    /// Indicates an invalid data URL
    InvalidDataUrl,
    /// Flatten failed
    CannotFlatten(String),
    /// The magic of a RAM bundle did not match
    InvalidRamBundleMagic,
    /// The RAM bundle index was malformed
    InvalidRamBundleIndex,
    /// A RAM bundle entry was invalid
    InvalidRamBundleEntry,
    /// Tried to operate on a non RAM bundle file
    NotARamBundle,
    /// Range mapping index is invalid
    InvalidRangeMappingIndex(data_encoding::DecodeError),

    InvalidBase64(char),
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Error {
        Error::Io(err)
    }
}

#[cfg(feature = "ram_bundle")]
impl From<scroll::Error> for Error {
    fn from(err: scroll::Error) -> Self {
        Error::Scroll(err)
    }
}

impl From<string::FromUtf8Error> for Error {
    fn from(err: string::FromUtf8Error) -> Error {
        From::from(err.utf8_error())
    }
}

impl From<str::Utf8Error> for Error {
    fn from(err: str::Utf8Error) -> Error {
        Error::Utf8(err)
    }
}

impl From<serde_json::Error> for Error {
    fn from(err: serde_json::Error) -> Error {
        Error::BadJson(err)
    }
}

impl From<data_encoding::DecodeError> for Error {
    fn from(err: data_encoding::DecodeError) -> Error {
        Error::InvalidRangeMappingIndex(err)
    }
}

impl error::Error for Error {
    fn cause(&self) -> Option<&dyn error::Error> {
        match *self {
            Error::Io(ref err) => Some(err),
            #[cfg(feature = "ram_bundle")]
            Error::Scroll(ref err) => Some(err),
            Error::Utf8(ref err) => Some(err),
            Error::BadJson(ref err) => Some(err),
            _ => None,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Error::Io(ref msg) => write!(f, "{msg}"),
            Error::Utf8(ref msg) => write!(f, "{msg}"),
            Error::BadJson(ref err) => write!(f, "bad json: {err}"),
            #[cfg(feature = "ram_bundle")]
            Error::Scroll(ref err) => write!(f, "parse error: {err}"),
            Error::VlqLeftover => write!(f, "leftover cur/shift in vlq decode"),
            Error::VlqNoValues => write!(f, "vlq decode did not produce any values"),
            Error::VlqOverflow => write!(f, "vlq decode caused an overflow"),
            Error::BadSegmentSize(size) => write!(f, "got {size} segments, expected 4 or 5"),
            Error::BadSourceReference(id) => write!(f, "bad reference to source #{id}"),
            Error::BadNameReference(id) => write!(f, "bad reference to name #{id}"),
            Error::IncompatibleSourceMap => write!(f, "encountered incompatible sourcemap format"),
            Error::InvalidDataUrl => write!(f, "the provided data URL is invalid"),
            Error::CannotFlatten(ref msg) => {
                write!(f, "cannot flatten the indexed sourcemap: {msg}")
            }
            Error::InvalidRamBundleMagic => write!(f, "invalid magic number for ram bundle"),
            Error::InvalidRamBundleIndex => write!(f, "invalid module index in ram bundle"),
            Error::InvalidRamBundleEntry => write!(f, "invalid ram bundle module entry"),
            Error::NotARamBundle => write!(f, "not a ram bundle"),
            Error::InvalidRangeMappingIndex(err) => write!(f, "invalid range mapping index: {err}"),
            Error::InvalidBase64(c) => write!(f, "invalid base64 character: {c}"),
        }
    }
}
