use std::{
    borrow::{Borrow, Cow},
    cmp::Ordering,
    ffi::OsStr,
    fmt::{self, Debug, Display},
    hash::{Hash, Hasher},
    ops::{Deref, Index, RangeBounds},
    path::Path,
    slice::SliceIndex,
    str::Utf8Error,
};

use bytes::{Buf, Bytes};

use crate::BytesString;

/// A reference-counted `str` backed by [Bytes].
///
/// Clone is cheap thanks to [Bytes].
///
///
/// # Features
///
/// ## `rkyv`
///
/// If the `rkyv` feature is enabled, the [BytesStr] type will be
/// [rkyv::Archive], [rkyv::Serialize], and [rkyv::Deserialize].
///
///
/// ## `serde`
///
/// If the `serde` feature is enabled, the [BytesStr] type will be
/// [serde::Serialize] and [serde::Deserialize].
///
/// The [BytesStr] type will be serialized as a [str] type.
#[derive(Clone, Default, PartialEq, Eq)]
#[cfg_attr(
    feature = "rkyv",
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
pub struct BytesStr {
    pub(crate) bytes: Bytes,
}

impl BytesStr {
    /// Creates a new empty BytesStr.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///
    /// let s = BytesStr::new();
    ///
    /// assert_eq!(s.as_str(), "");
    /// ```
    pub fn new() -> Self {
        Self {
            bytes: Bytes::new(),
        }
    }

    /// Creates a new BytesStr from a static string.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///
    /// let s = BytesStr::from_static("hello");
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_static(bytes: &'static str) -> Self {
        Self {
            bytes: Bytes::from_static(bytes.as_bytes()),
        }
    }

    /// Creates a new BytesStr from a [Bytes].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    /// use bytes::Bytes;
    ///
    /// let s = BytesStr::from_utf8(Bytes::from_static(b"hello")).unwrap();
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_utf8(bytes: Bytes) -> Result<Self, Utf8Error> {
        std::str::from_utf8(&bytes)?;

        Ok(Self { bytes })
    }

    /// Creates a new BytesStr from a [Vec<u8>].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    /// use bytes::Bytes;
    ///
    /// let s = BytesStr::from_utf8_vec(b"hello".to_vec()).unwrap();
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_utf8_vec(bytes: Vec<u8>) -> Result<Self, Utf8Error> {
        std::str::from_utf8(&bytes)?;

        Ok(Self {
            bytes: Bytes::from(bytes),
        })
    }

    /// Creates a new BytesStr from an owner.
    ///
    /// See [Bytes::from_owner] for more information.
    pub fn from_owned_utf8<T>(owner: T) -> Result<Self, Utf8Error>
    where
        T: AsRef<[u8]> + Send + 'static,
    {
        std::str::from_utf8(owner.as_ref())?;

        Ok(Self {
            bytes: Bytes::from_owner(owner),
        })
    }

    /// Creates a new BytesStr from a [Bytes] without checking if the bytes
    /// are valid UTF-8.
    ///
    /// # Safety
    ///
    /// This function is unsafe because it does not check if the bytes are valid
    /// UTF-8. If the bytes are not valid UTF-8, the resulting BytesStr will
    pub unsafe fn from_utf8_unchecked(bytes: Bytes) -> Self {
        Self { bytes }
    }

    /// Creates a new BytesStr from a [Vec<u8>] without checking if the bytes
    /// are valid UTF-8.
    ///
    /// # Safety
    ///
    /// This function is unsafe because it does not check if the bytes are valid
    /// UTF-8. If the bytes are not valid UTF-8, the resulting BytesStr will
    /// be invalid.
    pub unsafe fn from_utf8_vec_unchecked(bytes: Vec<u8>) -> Self {
        Self::from_utf8_unchecked(Bytes::from(bytes))
    }

    /// Creates a new BytesStr from a [Bytes].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    /// use bytes::Bytes;
    ///     
    /// let s = BytesStr::from_utf8_slice(b"hello").unwrap();
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_utf8_slice(bytes: &[u8]) -> Result<Self, Utf8Error> {
        std::str::from_utf8(bytes)?;

        Ok(Self {
            bytes: Bytes::copy_from_slice(bytes),
        })
    }

    /// Creates a new BytesStr from a [Bytes] without checking if the bytes
    /// are valid UTF-8.
    ///
    /// # Safety
    ///
    /// This function is unsafe because it does not check if the bytes are valid
    /// UTF-8. If the bytes are not valid UTF-8, the resulting BytesStr will
    /// be invalid.
    pub unsafe fn from_utf8_slice_unchecked(bytes: &[u8]) -> Self {
        Self {
            bytes: Bytes::copy_from_slice(bytes),
        }
    }

    /// Creates a new BytesStr from a [str].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let s = BytesStr::from_str_slice("hello");
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_str_slice(bytes: &str) -> Self {
        Self {
            bytes: Bytes::copy_from_slice(bytes.as_bytes()),
        }
    }

    /// Creates a new BytesStr from a [String].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///         
    /// let s = BytesStr::from_string("hello".to_string());
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_string(bytes: String) -> Self {
        Self {
            bytes: Bytes::from(bytes),
        }
    }

    /// Creates a new BytesStr from a static UTF-8 slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let s = BytesStr::from_static_utf8_slice(b"hello").unwrap();
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn from_static_utf8_slice(bytes: &'static [u8]) -> Result<Self, Utf8Error> {
        std::str::from_utf8(bytes)?;

        Ok(Self {
            bytes: Bytes::from_static(bytes),
        })
    }

    /// Creates a new BytesStr from a static UTF-8 slice without checking if the
    /// bytes are valid UTF-8.
    ///
    /// # Safety
    ///
    /// This function is unsafe because it does not check if the bytes are valid
    /// UTF-8. If the bytes are not valid UTF-8, the resulting BytesStr will
    /// be invalid.
    pub unsafe fn from_static_utf8_slice_unchecked(bytes: &'static [u8]) -> Self {
        Self {
            bytes: Bytes::from_static(bytes),
        }
    }

    /// Returns a string slice containing the entire BytesStr.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///
    /// let s = BytesStr::from_static("hello");
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn as_str(&self) -> &str {
        unsafe { std::str::from_utf8_unchecked(&self.bytes) }
    }

    /// Converts the [BytesStr] into a [Bytes].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    /// use bytes::Bytes;
    ///     
    /// let s = BytesStr::from_static("hello");
    /// let bytes = s.into_bytes();
    ///
    /// assert_eq!(bytes, Bytes::from_static(b"hello"));
    /// ```
    pub fn into_bytes(self) -> Bytes {
        self.bytes
    }

    /// Converts the [BytesStr] into a [Vec<u8>].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let s = BytesStr::from_static("hello");
    /// let vec = s.into_vec();
    ///
    /// assert_eq!(vec, b"hello");
    /// ```
    pub fn into_vec(self) -> Vec<u8> {
        self.into_bytes().to_vec()
    }

    /// Converts the [BytesStr] into a [String].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let s = BytesStr::from_static("hello");
    /// let string = s.into_string();
    ///
    /// assert_eq!(string, "hello");
    /// ```
    pub fn into_string(self) -> String {
        unsafe {
            // Safety: BytesStr is backed by a valid UTF-8 string.
            String::from_utf8_unchecked(self.into_vec())
        }
    }

    /// Returns the length of the [BytesStr].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///
    /// let s = BytesStr::from_static("hello");
    ///
    /// assert_eq!(s.len(), 5);
    /// ```
    pub const fn len(&self) -> usize {
        self.bytes.len()
    }

    /// Returns true if the [BytesStr] is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let s = BytesStr::new();
    ///
    /// assert!(s.is_empty());
    /// ```
    pub const fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }

    /// Returns a slice of the [BytesStr].
    ///
    /// # Panics
    ///
    /// Panics if the bounds are not character boundaries.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let s = BytesStr::from_static("hello");
    /// let slice = s.slice(1..3);
    ///
    /// assert_eq!(slice.as_str(), "el");
    /// ```
    pub fn slice(&self, range: impl RangeBounds<usize>) -> Self {
        let s = Self {
            bytes: self.bytes.slice(range),
        };

        if !s.is_char_boundary(0) {
            panic!("range start is not a character boundary");
        }

        if !s.is_char_boundary(s.len()) {
            panic!("range end is not a character boundary");
        }

        s
    }

    /// See [Bytes::slice_ref]
    pub fn slice_ref(&self, subset: &str) -> Self {
        Self {
            bytes: self.bytes.slice_ref(subset.as_bytes()),
        }
    }

    /// Advances the [BytesStr] by `n` bytes.
    ///
    /// # Panics
    ///
    /// Panics if `n` is greater than the length of the [BytesStr].
    ///
    /// Panics if `n` is not a character boundary.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesStr;
    ///     
    /// let mut s = BytesStr::from_static("hello");
    /// s.advance(3);
    ///
    /// assert_eq!(s.as_str(), "lo");
    /// ```
    pub fn advance(&mut self, n: usize) {
        if !self.is_char_boundary(n) {
            panic!("n is not a character boundary");
        }

        self.bytes.advance(n);
    }
}

impl Deref for BytesStr {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        self.as_ref()
    }
}

impl AsRef<str> for BytesStr {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl From<String> for BytesStr {
    fn from(s: String) -> Self {
        Self {
            bytes: Bytes::from(s),
        }
    }
}

impl From<&'static str> for BytesStr {
    fn from(s: &'static str) -> Self {
        Self {
            bytes: Bytes::from_static(s.as_bytes()),
        }
    }
}

impl From<BytesStr> for BytesString {
    fn from(s: BytesStr) -> Self {
        Self {
            bytes: s.bytes.into(),
        }
    }
}

impl From<BytesString> for BytesStr {
    fn from(s: BytesString) -> Self {
        Self {
            bytes: s.bytes.into(),
        }
    }
}

impl AsRef<[u8]> for BytesStr {
    fn as_ref(&self) -> &[u8] {
        self.bytes.as_ref()
    }
}

impl AsRef<Bytes> for BytesStr {
    fn as_ref(&self) -> &Bytes {
        &self.bytes
    }
}

impl AsRef<OsStr> for BytesStr {
    fn as_ref(&self) -> &OsStr {
        OsStr::new(self.as_str())
    }
}

impl AsRef<Path> for BytesStr {
    fn as_ref(&self) -> &Path {
        Path::new(self.as_str())
    }
}

impl Borrow<str> for BytesStr {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl Debug for BytesStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(self.as_str(), f)
    }
}

impl Display for BytesStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(self.as_str(), f)
    }
}

impl Extend<BytesStr> for BytesString {
    fn extend<T: IntoIterator<Item = BytesStr>>(&mut self, iter: T) {
        self.bytes.extend(iter.into_iter().map(|s| s.bytes));
    }
}

impl<I> Index<I> for BytesStr
where
    I: SliceIndex<str>,
{
    type Output = I::Output;

    fn index(&self, index: I) -> &Self::Output {
        self.as_str().index(index)
    }
}

impl PartialEq<str> for BytesStr {
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}

impl PartialEq<&'_ str> for BytesStr {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<Cow<'_, str>> for BytesStr {
    fn eq(&self, other: &Cow<'_, str>) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<BytesStr> for str {
    fn eq(&self, other: &BytesStr) -> bool {
        self == other.as_str()
    }
}

impl PartialEq<BytesStr> for &'_ str {
    fn eq(&self, other: &BytesStr) -> bool {
        *self == other.as_str()
    }
}

impl PartialEq<BytesStr> for Bytes {
    fn eq(&self, other: &BytesStr) -> bool {
        *self == other.bytes
    }
}

impl PartialEq<String> for BytesStr {
    fn eq(&self, other: &String) -> bool {
        self.as_str() == other
    }
}

impl PartialEq<BytesStr> for String {
    fn eq(&self, other: &BytesStr) -> bool {
        self == other.as_str()
    }
}

impl Ord for BytesStr {
    fn cmp(&self, other: &Self) -> Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl PartialOrd for BytesStr {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

/// This produces the same hash as [str]
impl Hash for BytesStr {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_str().hash(state);
    }
}

impl TryFrom<&'static [u8]> for BytesStr {
    type Error = Utf8Error;

    fn try_from(value: &'static [u8]) -> Result<Self, Self::Error> {
        Self::from_static_utf8_slice(value)
    }
}

#[cfg(feature = "serde")]
mod serde_impl {
    use serde::{Deserialize, Deserializer, Serialize, Serializer};

    use super::*;

    impl<'de> Deserialize<'de> for BytesStr {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            let s = String::deserialize(deserializer)?;
            Ok(Self::from(s))
        }
    }

    impl Serialize for BytesStr {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: Serializer,
        {
            serializer.serialize_str(self.as_str())
        }
    }
}

#[cfg(test)]
mod tests {
    use std::{
        borrow::{Borrow, Cow},
        collections::{hash_map::DefaultHasher, HashMap},
        ffi::OsStr,
        hash::{Hash, Hasher},
        path::Path,
    };

    use bytes::Bytes;

    use super::*;
    use crate::BytesString;

    #[test]
    fn test_new() {
        let s = BytesStr::new();
        assert_eq!(s.as_str(), "");
        assert_eq!(s.len(), 0);
        assert!(s.is_empty());
    }

    #[test]
    fn test_default() {
        let s: BytesStr = Default::default();
        assert_eq!(s.as_str(), "");
        assert_eq!(s.len(), 0);
        assert!(s.is_empty());
    }

    #[test]
    fn test_from_static() {
        let s = BytesStr::from_static("hello world");
        assert_eq!(s.as_str(), "hello world");
        assert_eq!(s.len(), 11);
        assert!(!s.is_empty());

        // Test with unicode
        let s = BytesStr::from_static("한국어 🌍");
        assert_eq!(s.as_str(), "한국어 🌍");
    }

    #[test]
    fn test_from_utf8() {
        let bytes = Bytes::from_static(b"hello");
        let s = BytesStr::from_utf8(bytes).unwrap();
        assert_eq!(s.as_str(), "hello");

        // Test with unicode
        let bytes = Bytes::from("한국어".as_bytes());
        let s = BytesStr::from_utf8(bytes).unwrap();
        assert_eq!(s.as_str(), "한국어");

        // Test with invalid UTF-8
        let invalid_bytes = Bytes::from_static(&[0xff, 0xfe]);
        assert!(BytesStr::from_utf8(invalid_bytes).is_err());
    }

    #[test]
    fn test_from_utf8_vec() {
        let vec = b"hello world".to_vec();
        let s = BytesStr::from_utf8_vec(vec).unwrap();
        assert_eq!(s.as_str(), "hello world");

        // Test with unicode
        let vec = "한국어 🎉".as_bytes().to_vec();
        let s = BytesStr::from_utf8_vec(vec).unwrap();
        assert_eq!(s.as_str(), "한국어 🎉");

        // Test with invalid UTF-8
        let invalid_vec = vec![0xff, 0xfe];
        assert!(BytesStr::from_utf8_vec(invalid_vec).is_err());
    }

    #[test]
    fn test_from_utf8_unchecked() {
        let bytes = Bytes::from_static(b"hello");
        let s = unsafe { BytesStr::from_utf8_unchecked(bytes) };
        assert_eq!(s.as_str(), "hello");

        // Test with unicode
        let bytes = Bytes::from("한국어".as_bytes());
        let s = unsafe { BytesStr::from_utf8_unchecked(bytes) };
        assert_eq!(s.as_str(), "한국어");
    }

    #[test]
    fn test_from_utf8_vec_unchecked() {
        let vec = b"hello world".to_vec();
        let s = unsafe { BytesStr::from_utf8_vec_unchecked(vec) };
        assert_eq!(s.as_str(), "hello world");

        // Test with unicode
        let vec = "한국어 🎉".as_bytes().to_vec();
        let s = unsafe { BytesStr::from_utf8_vec_unchecked(vec) };
        assert_eq!(s.as_str(), "한국어 🎉");
    }

    #[test]
    fn test_from_utf8_slice() {
        let s = BytesStr::from_utf8_slice(b"hello").unwrap();
        assert_eq!(s.as_str(), "hello");

        // Test with unicode
        let s = BytesStr::from_utf8_slice("한국어".as_bytes()).unwrap();
        assert_eq!(s.as_str(), "한국어");

        // Test with invalid UTF-8
        assert!(BytesStr::from_utf8_slice(&[0xff, 0xfe]).is_err());
    }

    #[test]
    fn test_from_utf8_slice_unchecked() {
        let s = unsafe { BytesStr::from_utf8_slice_unchecked(b"hello") };
        assert_eq!(s.as_str(), "hello");

        // Test with unicode
        let s = unsafe { BytesStr::from_utf8_slice_unchecked("한국어".as_bytes()) };
        assert_eq!(s.as_str(), "한국어");
    }

    #[test]
    fn test_from_static_utf8_slice() {
        let s = BytesStr::from_static_utf8_slice(b"hello").unwrap();
        assert_eq!(s.as_str(), "hello");

        // Test with unicode
        let s = BytesStr::from_static_utf8_slice("한국어".as_bytes()).unwrap();
        assert_eq!(s.as_str(), "한국어");

        // Test with invalid UTF-8
        assert!(BytesStr::from_static_utf8_slice(&[0xff, 0xfe]).is_err());
    }

    #[test]
    fn test_from_static_utf8_slice_unchecked() {
        let s = unsafe { BytesStr::from_static_utf8_slice_unchecked(b"hello") };
        assert_eq!(s.as_str(), "hello");

        // Test with unicode
        let s = unsafe { BytesStr::from_static_utf8_slice_unchecked("한국어".as_bytes()) };
        assert_eq!(s.as_str(), "한국어");
    }

    #[test]
    fn test_as_str() {
        let s = BytesStr::from_static("hello world");
        assert_eq!(s.as_str(), "hello world");

        // Test with unicode
        let s = BytesStr::from_static("한국어 🌍");
        assert_eq!(s.as_str(), "한국어 🌍");
    }

    #[test]
    fn test_deref() {
        let s = BytesStr::from_static("hello world");

        // Test that we can call str methods directly
        assert_eq!(s.len(), 11);
        assert!(s.contains("world"));
        assert!(s.starts_with("hello"));
        assert!(s.ends_with("world"));
        assert_eq!(&s[0..5], "hello");
    }

    #[test]
    fn test_as_ref_str() {
        let s = BytesStr::from_static("hello");
        let str_ref: &str = s.as_ref();
        assert_eq!(str_ref, "hello");
    }

    #[test]
    fn test_as_ref_bytes() {
        let s = BytesStr::from_static("hello");
        let bytes_ref: &[u8] = s.as_ref();
        assert_eq!(bytes_ref, b"hello");
    }

    #[test]
    fn test_as_ref_bytes_type() {
        let s = BytesStr::from_static("hello");
        let bytes_ref: &Bytes = s.as_ref();
        assert_eq!(bytes_ref.as_ref(), b"hello");
    }

    #[test]
    fn test_as_ref_os_str() {
        let s = BytesStr::from_static("hello/world");
        let os_str_ref: &OsStr = s.as_ref();
        assert_eq!(os_str_ref, OsStr::new("hello/world"));
    }

    #[test]
    fn test_as_ref_path() {
        let s = BytesStr::from_static("hello/world");
        let path_ref: &Path = s.as_ref();
        assert_eq!(path_ref, Path::new("hello/world"));
    }

    #[test]
    fn test_borrow() {
        let s = BytesStr::from_static("hello");
        let borrowed: &str = s.borrow();
        assert_eq!(borrowed, "hello");
    }

    #[test]
    fn test_from_string() {
        let original = String::from("hello world");
        let s = BytesStr::from(original);
        assert_eq!(s.as_str(), "hello world");
    }

    #[test]
    fn test_from_static_str() {
        let s = BytesStr::from("hello world");
        assert_eq!(s.as_str(), "hello world");
    }

    #[test]
    fn test_conversion_to_bytes_string() {
        let s = BytesStr::from_static("hello");
        let bytes_string: BytesString = s.into();
        assert_eq!(bytes_string.as_str(), "hello");
    }

    #[test]
    fn test_conversion_from_bytes_string() {
        let mut bytes_string = BytesString::from("hello");
        bytes_string.push_str(" world");
        let s: BytesStr = bytes_string.into();
        assert_eq!(s.as_str(), "hello world");
    }

    #[test]
    fn test_try_from_static_slice() {
        let s = BytesStr::try_from(b"hello" as &'static [u8]).unwrap();
        assert_eq!(s.as_str(), "hello");

        // Test with invalid UTF-8
        let invalid_slice: &'static [u8] = &[0xff, 0xfe];
        assert!(BytesStr::try_from(invalid_slice).is_err());
    }

    #[test]
    fn test_debug() {
        let s = BytesStr::from_static("hello");
        assert_eq!(format!("{:?}", s), "\"hello\"");

        let s = BytesStr::from_static("hello\nworld");
        assert_eq!(format!("{:?}", s), "\"hello\\nworld\"");
    }

    #[test]
    fn test_display() {
        let s = BytesStr::from_static("hello world");
        assert_eq!(format!("{}", s), "hello world");

        let s = BytesStr::from_static("한국어 🌍");
        assert_eq!(format!("{}", s), "한국어 🌍");
    }

    #[test]
    fn test_index() {
        let s = BytesStr::from_static("hello world");
        assert_eq!(&s[0..5], "hello");
        assert_eq!(&s[6..], "world");
        assert_eq!(&s[..5], "hello");
        assert_eq!(&s[6..11], "world");

        // Test with unicode
        let s = BytesStr::from_static("한국어");
        assert_eq!(&s[0..6], "한국");
    }

    #[test]
    fn test_partial_eq_str() {
        let s = BytesStr::from_static("hello");

        // Test BytesStr == str
        assert_eq!(s, "hello");
        assert_ne!(s, "world");

        // Test str == BytesStr
        assert_eq!("hello", s);
        assert_ne!("world", s);

        // Test BytesStr == &str
        let hello_str = "hello";
        let world_str = "world";
        assert_eq!(s, hello_str);
        assert_ne!(s, world_str);

        // Test &str == BytesStr
        assert_eq!(hello_str, s);
        assert_ne!(world_str, s);
    }

    #[test]
    fn test_partial_eq_string() {
        let s = BytesStr::from_static("hello");
        let string = String::from("hello");
        let other_string = String::from("world");

        // Test BytesStr == String
        assert_eq!(s, string);
        assert_ne!(s, other_string);

        // Test String == BytesStr
        assert_eq!(string, s);
        assert_ne!(other_string, s);
    }

    #[test]
    fn test_partial_eq_cow() {
        let s = BytesStr::from_static("hello");

        assert_eq!(s, Cow::Borrowed("hello"));
        assert_eq!(s, Cow::Owned(String::from("hello")));
        assert_ne!(s, Cow::Borrowed("world"));
        assert_ne!(s, Cow::Owned(String::from("world")));
    }

    #[test]
    fn test_partial_eq_bytes() {
        let s = BytesStr::from_static("hello");
        let bytes = Bytes::from_static(b"hello");
        let other_bytes = Bytes::from_static(b"world");

        assert_eq!(bytes, s);
        assert_ne!(other_bytes, s);
    }

    #[test]
    fn test_partial_eq_bytes_str() {
        let s1 = BytesStr::from_static("hello");
        let s2 = BytesStr::from_static("hello");
        let s3 = BytesStr::from_static("world");

        assert_eq!(s1, s2);
        assert_ne!(s1, s3);
    }

    #[test]
    fn test_ordering() {
        let s1 = BytesStr::from_static("apple");
        let s2 = BytesStr::from_static("banana");
        let s3 = BytesStr::from_static("apple");

        assert!(s1 < s2);
        assert!(s2 > s1);
        assert_eq!(s1, s3);
        assert!(s1 <= s3);
        assert!(s1 >= s3);

        // Test partial_cmp
        assert_eq!(s1.partial_cmp(&s2), Some(std::cmp::Ordering::Less));
        assert_eq!(s2.partial_cmp(&s1), Some(std::cmp::Ordering::Greater));
        assert_eq!(s1.partial_cmp(&s3), Some(std::cmp::Ordering::Equal));
    }

    #[test]
    fn test_hash() {
        let s1 = BytesStr::from_static("hello");
        let s2 = BytesStr::from_static("hello");
        let s3 = BytesStr::from_static("world");

        let mut hasher1 = DefaultHasher::new();
        let mut hasher2 = DefaultHasher::new();
        let mut hasher3 = DefaultHasher::new();

        s1.hash(&mut hasher1);
        s2.hash(&mut hasher2);
        s3.hash(&mut hasher3);

        assert_eq!(hasher1.finish(), hasher2.finish());
        assert_ne!(hasher1.finish(), hasher3.finish());

        // Test hash consistency with str
        let mut str_hasher = DefaultHasher::new();
        "hello".hash(&mut str_hasher);
        assert_eq!(hasher1.finish(), str_hasher.finish());
    }

    #[test]
    fn test_clone() {
        let s1 = BytesStr::from_static("hello world");
        let s2 = s1.clone();

        assert_eq!(s1, s2);
        assert_eq!(s1.as_str(), s2.as_str());

        // Clone should be cheap (reference counting)
        // Both should point to the same underlying data
    }

    #[test]
    fn test_extend_bytes_string() {
        let mut bytes_string = BytesString::from("hello");
        let parts = vec![
            BytesStr::from_static(" "),
            BytesStr::from_static("world"),
            BytesStr::from_static("!"),
        ];

        bytes_string.extend(parts);
        assert_eq!(bytes_string.as_str(), "hello world!");
    }

    #[test]
    fn test_unicode_handling() {
        let s = BytesStr::from_static("Hello 🌍 한국어 🎉");
        assert_eq!(s.as_str(), "Hello 🌍 한국어 🎉");
        assert!(s.len() > 13); // More than ASCII length due to UTF-8 encoding

        // Test indexing with unicode (be careful with boundaries)
        let korean = BytesStr::from_static("한국어");
        assert_eq!(korean.len(), 9); // 3 characters * 3 bytes each
        assert_eq!(&korean[0..6], "한국"); // First two characters
    }

    #[test]
    fn test_empty_strings() {
        let s = BytesStr::new();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
        assert_eq!(s.as_str(), "");

        let s = BytesStr::from_static("");
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
        assert_eq!(s.as_str(), "");
    }

    #[test]
    fn test_large_strings() {
        let large_str = "a".repeat(10000);
        let s = BytesStr::from(large_str.clone());
        assert_eq!(s.len(), 10000);
        assert_eq!(s.as_str(), large_str);
    }

    #[test]
    fn test_hash_map_usage() {
        let mut map = HashMap::new();
        let key = BytesStr::from_static("key");
        map.insert(key, "value");

        let lookup_key = BytesStr::from_static("key");
        assert_eq!(map.get(&lookup_key), Some(&"value"));

        // Test that string can be used to lookup BytesStr key
        assert_eq!(map.get("key"), Some(&"value"));
    }

    #[test]
    fn test_memory_efficiency() {
        // Test that cloning is cheap (reference counting)
        let original = BytesStr::from(String::from("hello world"));
        let clone1 = original.clone();
        let clone2 = original.clone();

        // All should have the same content
        assert_eq!(original.as_str(), "hello world");
        assert_eq!(clone1.as_str(), "hello world");
        assert_eq!(clone2.as_str(), "hello world");

        // They should be equal
        assert_eq!(original, clone1);
        assert_eq!(clone1, clone2);
    }

    #[test]
    fn test_static_vs_owned() {
        // Test static string
        let static_str = BytesStr::from_static("hello");
        assert_eq!(static_str.as_str(), "hello");

        // Test owned string
        let owned_str = BytesStr::from(String::from("hello"));
        assert_eq!(owned_str.as_str(), "hello");

        // They should be equal even if one is static and one is owned
        assert_eq!(static_str, owned_str);
    }

    #[test]
    fn test_error_cases() {
        // Test various invalid UTF-8 sequences
        let invalid_sequences = vec![
            vec![0xff],             // Invalid start byte
            vec![0xfe, 0xff],       // Invalid sequence
            vec![0xc0, 0x80],       // Overlong encoding
            vec![0xe0, 0x80, 0x80], // Overlong encoding
        ];

        for invalid in invalid_sequences {
            assert!(BytesStr::from_utf8(Bytes::from(invalid.clone())).is_err());
            assert!(BytesStr::from_utf8_vec(invalid.clone()).is_err());
            assert!(BytesStr::from_utf8_slice(&invalid).is_err());
        }
    }

    #[test]
    fn test_boundary_conditions() {
        // Test with single character
        let s = BytesStr::from_static("a");
        assert_eq!(s.len(), 1);
        assert_eq!(s.as_str(), "a");

        // Test with single unicode character
        let s = BytesStr::from_static("한");
        assert_eq!(s.len(), 3); // UTF-8 encoding
        assert_eq!(s.as_str(), "한");

        // Test with emoji
        let s = BytesStr::from_static("🌍");
        assert_eq!(s.len(), 4); // UTF-8 encoding
        assert_eq!(s.as_str(), "🌍");
    }
}
