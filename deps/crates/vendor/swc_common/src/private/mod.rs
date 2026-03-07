//! This module is private module and can be changed without notice.

pub mod content;

// Re-export serde utilities that we need
pub mod serde {
    pub type Result<T, E = String> = std::result::Result<T, E>;
    pub type Formatter<'a> = std::fmt::Formatter<'a>;
    pub use std::fmt;
    // Re-export Result constructors
    pub use std::result::Result::{Err, Ok};

    // Re-export our custom Content types
    pub mod de {
        pub use super::super::content::{
            deserialize_content, Content, ContentDeserializer, ContentRefDeserializer,
            ContentVisitor,
        };
    }

    // Helper function for error messages
    pub fn from_utf8_lossy(bytes: &[u8]) -> std::borrow::Cow<'_, str> {
        String::from_utf8_lossy(bytes)
    }
}
