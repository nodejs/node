use std::{
    borrow::{Borrow, BorrowMut, Cow},
    cmp::Ordering,
    convert::Infallible,
    ffi::OsStr,
    fmt::{self, Debug, Display},
    hash::{Hash, Hasher},
    net::{SocketAddr, ToSocketAddrs},
    ops::{Add, AddAssign, Deref, DerefMut, Index, IndexMut},
    path::Path,
    slice::SliceIndex,
    str::{FromStr, Utf8Error},
};

use bytes::{Bytes, BytesMut};

/// [String] but backed by a [BytesMut]
///
/// # Features
///
/// ## `serde`
///
/// If the `serde` feature is enabled, the [BytesString] type will be
/// [serde::Serialize] and [serde::Deserialize].
///
/// The [BytesString] type will be serialized just like a [String] type.
#[derive(Clone, Default, PartialEq, Eq)]
pub struct BytesString {
    pub(crate) bytes: BytesMut,
}

impl BytesString {
    /// Returns a new, empty BytesString.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::new();
    ///
    /// assert!(s.is_empty());
    /// ```
    pub fn new() -> Self {
        Self {
            bytes: BytesMut::new(),
        }
    }

    /// Returns a new, empty BytesString with the specified capacity.
    ///
    /// The capacity is the size of the internal buffer in bytes.
    ///
    /// The actual capacity may be larger than the specified capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::with_capacity(10);
    ///
    /// assert!(s.capacity() >= 10);
    /// ```
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            bytes: BytesMut::with_capacity(capacity),
        }
    }

    /// Returns the length of this String, in bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::from("hello");
    ///
    /// assert_eq!(s.len(), 5);
    /// ```
    pub fn len(&self) -> usize {
        self.bytes.len()
    }

    /// Returns the capacity of this String, in bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::from("hello");
    ///
    /// assert!(s.capacity() >= 5);
    /// ```
    pub fn capacity(&self) -> usize {
        self.bytes.capacity()
    }

    /// Reserves the minimum capacity for exactly `additional` more bytes to be
    /// stored without reallocating.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows usize.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// s.reserve(10);
    ///
    /// assert!(s.capacity() >= 15);
    /// ```
    pub fn reserve(&mut self, additional: usize) {
        self.bytes.reserve(additional);
    }

    /// Splits the string into two at the given index.
    ///
    /// Returns a newly allocated String. `self` contains bytes at indices
    /// greater than `at`, and the returned string contains bytes at indices
    /// less than `at`.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// let other = s.split_off(2);
    ///
    /// assert_eq!(s, "he");
    /// assert_eq!(other, "llo");
    /// ```
    pub fn split_off(&mut self, at: usize) -> Self {
        Self {
            bytes: self.bytes.split_off(at),
        }
    }

    /// Returns a byte slice of this String's contents.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::from("hello");
    ///
    /// assert_eq!(s.as_bytes(), b"hello");
    /// ```
    pub fn as_bytes(&self) -> &[u8] {
        self.bytes.as_ref()
    }

    /// Returns true if the BytesString has a length of 0.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::new();
    ///
    /// assert!(s.is_empty());
    /// ```
    pub fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }

    /// Truncates the BytesString to the specified length.
    ///
    /// If new_len is greater than or equal to the string's current length, this
    /// has no effect.
    ///
    /// Note that this method has no effect on the allocated capacity of the
    /// string
    ///
    /// # Arguments
    ///
    /// * `new_len` - The new length of the BytesString
    ///
    /// # Panics
    ///
    /// Panics if new_len does not lie on a char boundary.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// s.truncate(3);
    ///
    /// assert_eq!(s, "hel");
    /// ```
    ///
    ///
    /// Shortens this String to the specified length.
    pub fn truncate(&mut self, new_len: usize) {
        if new_len <= self.len() {
            assert!(self.is_char_boundary(new_len));
            self.bytes.truncate(new_len);
        }
    }

    /// Clears the BytesString, removing all bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// s.clear();
    ///
    /// assert!(s.is_empty());
    /// ```
    pub fn clear(&mut self) {
        self.bytes.clear();
    }

    /// Appends a character to the end of this BytesString.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// s.push(' ');
    ///
    /// assert_eq!(s, "hello ");
    /// ```
    pub fn push(&mut self, ch: char) {
        let mut buf = [0; 4];
        let bytes = ch.encode_utf8(&mut buf);
        self.bytes.extend_from_slice(bytes.as_bytes());
    }

    /// Appends a string slice to the end of this BytesString.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// s.push_str(" world");
    ///
    /// assert_eq!(s, "hello world");
    /// ```
    pub fn push_str(&mut self, s: &str) {
        self.bytes.extend_from_slice(s.as_bytes());
    }

    /// Returns a string slice containing the entire BytesString.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::from("hello");
    ///
    /// assert_eq!(s.as_str(), "hello");
    /// ```
    pub fn as_str(&self) -> &str {
        unsafe { std::str::from_utf8_unchecked(&self.bytes) }
    }

    /// Returns a mutable string slice containing the entire BytesString.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let mut s = BytesString::from("hello");
    ///
    /// s.as_mut_str().make_ascii_uppercase();
    ///
    /// assert_eq!(s, "HELLO");
    /// ```
    pub fn as_mut_str(&mut self) -> &mut str {
        unsafe { std::str::from_utf8_unchecked_mut(&mut self.bytes) }
    }

    /// Converts the BytesString into a [BytesMut].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    /// use bytes::BytesMut;
    ///
    /// let s = BytesString::from("hello");
    ///
    /// let bytes = s.into_bytes();
    ///
    /// assert_eq!(bytes, BytesMut::from(&b"hello"[..]));
    /// ```
    pub fn into_bytes(self) -> BytesMut {
        self.bytes
    }

    /// Converts a [BytesMut] into a [BytesString] without checking if the bytes
    /// are valid UTF-8.
    ///
    /// # Safety
    ///
    /// This function is unsafe because it does not check if the bytes are valid
    /// UTF-8.
    pub unsafe fn from_bytes_unchecked(bytes: BytesMut) -> Self {
        Self { bytes }
    }

    /// Converts a [BytesMut] into a [BytesString] if the bytes are valid UTF-8.
    ///
    /// # Errors
    ///
    /// Returns a [Utf8Error] if the bytes are not valid UTF-8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    /// use bytes::BytesMut;
    ///
    /// let s = BytesString::from_utf8(BytesMut::from(&b"hello"[..]));
    /// ```
    pub fn from_utf8(bytes: BytesMut) -> Result<Self, Utf8Error> {
        std::str::from_utf8(bytes.as_ref())?;

        Ok(Self { bytes })
    }

    /// Converts a slice of bytes into a [BytesString] if the bytes are valid
    /// UTF-8.
    ///
    /// # Errors
    ///
    /// Returns a [Utf8Error] if the bytes are not valid UTF-8.
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::from_utf8_slice(b"hello");
    /// ```
    pub fn from_utf8_slice(bytes: &[u8]) -> Result<Self, Utf8Error> {
        std::str::from_utf8(bytes)?;

        Ok(Self {
            bytes: BytesMut::from(bytes),
        })
    }

    /// Converts the [BytesString] into a [Vec<u8>].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    ///     
    /// let s = BytesString::from("hello");
    /// let vec = s.into_vec();
    ///
    /// assert_eq!(vec, b"hello");
    /// ```
    pub fn into_vec(self) -> Vec<u8> {
        self.bytes.into()
    }

    /// Converts a [BytesString] into a [String].
    ///
    /// # Examples
    ///
    /// ```
    /// use bytes_str::BytesString;
    /// ```
    /// use bytes_str::BytesString;
    ///
    /// let s = BytesString::from("hello");
    ///
    /// let string = s.into_string();
    ///
    /// assert_eq!(string, "hello");
    pub fn into_string(self) -> String {
        self.into()
    }
}

impl Deref for BytesString {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        self.as_str()
    }
}

impl DerefMut for BytesString {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut_str()
    }
}

impl AsRef<str> for BytesString {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl Borrow<str> for BytesString {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl From<String> for BytesString {
    fn from(s: String) -> Self {
        Self {
            bytes: Bytes::from(s.into_bytes()).into(),
        }
    }
}

impl From<BytesString> for String {
    fn from(s: BytesString) -> Self {
        let vec: Vec<_> = s.bytes.freeze().into();

        unsafe {
            // SAFETY: We know the bytes are valid UTF-8 because we created them
            String::from_utf8_unchecked(vec)
        }
    }
}

impl From<&str> for BytesString {
    fn from(s: &str) -> Self {
        Self {
            bytes: BytesMut::from(s),
        }
    }
}

impl From<BytesString> for BytesMut {
    fn from(s: BytesString) -> Self {
        s.bytes
    }
}

impl From<BytesString> for Bytes {
    fn from(s: BytesString) -> Self {
        s.bytes.into()
    }
}

impl From<char> for BytesString {
    fn from(ch: char) -> Self {
        let mut bytes = BytesString::with_capacity(ch.len_utf8());
        bytes.push(ch);
        bytes
    }
}

impl PartialEq<str> for BytesString {
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}

impl PartialEq<&'_ str> for BytesString {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<Cow<'_, str>> for BytesString {
    fn eq(&self, other: &Cow<'_, str>) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<BytesString> for str {
    fn eq(&self, other: &BytesString) -> bool {
        self == other.as_str()
    }
}

impl PartialEq<BytesString> for &'_ str {
    fn eq(&self, other: &BytesString) -> bool {
        *self == other.as_str()
    }
}

impl PartialEq<BytesString> for Bytes {
    fn eq(&self, other: &BytesString) -> bool {
        self == other.as_bytes()
    }
}

impl PartialEq<String> for BytesString {
    fn eq(&self, other: &String) -> bool {
        self.as_str() == other
    }
}

impl PartialEq<BytesString> for String {
    fn eq(&self, other: &BytesString) -> bool {
        self == other.as_str()
    }
}

impl Add<&str> for BytesString {
    type Output = Self;

    fn add(mut self, other: &str) -> Self::Output {
        self += other;
        self
    }
}

impl AddAssign<&str> for BytesString {
    fn add_assign(&mut self, other: &str) {
        self.push_str(other);
    }
}

impl Add<BytesString> for BytesString {
    type Output = Self;

    fn add(mut self, other: BytesString) -> Self::Output {
        self += other;
        self
    }
}

impl AddAssign<BytesString> for BytesString {
    fn add_assign(&mut self, other: BytesString) {
        self.bytes.extend(other.bytes);
    }
}

impl AsMut<str> for BytesString {
    fn as_mut(&mut self) -> &mut str {
        self.as_mut_str()
    }
}

impl AsRef<[u8]> for BytesString {
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl AsRef<OsStr> for BytesString {
    fn as_ref(&self) -> &OsStr {
        OsStr::new(self.as_str())
    }
}

impl AsRef<Path> for BytesString {
    fn as_ref(&self) -> &Path {
        Path::new(self.as_str())
    }
}

impl BorrowMut<str> for BytesString {
    fn borrow_mut(&mut self) -> &mut str {
        self.as_mut_str()
    }
}

impl Debug for BytesString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(self.as_str(), f)
    }
}

impl Display for BytesString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(self.as_str(), f)
    }
}

impl PartialOrd for BytesString {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for BytesString {
    fn cmp(&self, other: &Self) -> Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl<'a> Extend<&'a char> for BytesString {
    fn extend<T: IntoIterator<Item = &'a char>>(&mut self, iter: T) {
        self.extend(iter.into_iter().copied());
    }
}

impl Extend<char> for BytesString {
    fn extend<T: IntoIterator<Item = char>>(&mut self, iter: T) {
        let mut buf = [0; 4];
        for ch in iter {
            let bytes = ch.encode_utf8(&mut buf);
            self.bytes.extend_from_slice(bytes.as_bytes());
        }
    }
}

impl<'a> Extend<&'a str> for BytesString {
    fn extend<T: IntoIterator<Item = &'a str>>(&mut self, iter: T) {
        for s in iter {
            self.push_str(s);
        }
    }
}

impl<'a, 'b> Extend<&'a &'b str> for BytesString {
    fn extend<T: IntoIterator<Item = &'a &'b str>>(&mut self, iter: T) {
        for s in iter {
            self.push_str(s);
        }
    }
}

impl Extend<Box<str>> for BytesString {
    fn extend<T: IntoIterator<Item = Box<str>>>(&mut self, iter: T) {
        for s in iter {
            self.push_str(&s);
        }
    }
}

impl<'a> Extend<Cow<'a, str>> for BytesString {
    fn extend<T: IntoIterator<Item = Cow<'a, str>>>(&mut self, iter: T) {
        for s in iter {
            self.push_str(&s);
        }
    }
}

impl Extend<String> for BytesString {
    fn extend<T: IntoIterator<Item = String>>(&mut self, iter: T) {
        for s in iter {
            self.push_str(&s);
        }
    }
}

impl<'a> Extend<&'a String> for BytesString {
    fn extend<T: IntoIterator<Item = &'a String>>(&mut self, iter: T) {
        for s in iter {
            self.push_str(s);
        }
    }
}

impl Extend<BytesString> for BytesString {
    fn extend<T: IntoIterator<Item = BytesString>>(&mut self, iter: T) {
        for s in iter {
            self.bytes.extend(s.bytes);
        }
    }
}

impl FromIterator<char> for BytesString {
    fn from_iter<T: IntoIterator<Item = char>>(iter: T) -> Self {
        let mut bytes = BytesString::new();
        bytes.extend(iter);
        bytes
    }
}

impl<'a> FromIterator<&'a str> for BytesString {
    fn from_iter<T: IntoIterator<Item = &'a str>>(iter: T) -> Self {
        let mut bytes = BytesString::new();
        bytes.extend(iter);
        bytes
    }
}

impl FromIterator<Box<str>> for BytesString {
    fn from_iter<T: IntoIterator<Item = Box<str>>>(iter: T) -> Self {
        let mut bytes = BytesString::new();
        bytes.extend(iter);
        bytes
    }
}

impl<'a> FromIterator<Cow<'a, str>> for BytesString {
    fn from_iter<T: IntoIterator<Item = Cow<'a, str>>>(iter: T) -> Self {
        let mut bytes = BytesString::new();
        bytes.extend(iter);
        bytes
    }
}

impl FromIterator<String> for BytesString {
    fn from_iter<T: IntoIterator<Item = String>>(iter: T) -> Self {
        let mut bytes = BytesString::new();
        bytes.extend(iter);
        bytes
    }
}

impl FromIterator<BytesString> for BytesString {
    fn from_iter<T: IntoIterator<Item = BytesString>>(iter: T) -> Self {
        let mut bytes = BytesString::new();
        bytes.extend(iter);
        bytes
    }
}

impl FromStr for BytesString {
    type Err = Infallible;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Self {
            bytes: BytesMut::from(s),
        })
    }
}

impl<I> Index<I> for BytesString
where
    I: SliceIndex<str>,
{
    type Output = I::Output;

    fn index(&self, index: I) -> &Self::Output {
        self.as_str().index(index)
    }
}

impl<I> IndexMut<I> for BytesString
where
    I: SliceIndex<str>,
{
    fn index_mut(&mut self, index: I) -> &mut Self::Output {
        self.as_mut_str().index_mut(index)
    }
}

impl ToSocketAddrs for BytesString {
    type Iter = std::vec::IntoIter<SocketAddr>;

    fn to_socket_addrs(&self) -> Result<Self::Iter, std::io::Error> {
        self.as_str().to_socket_addrs()
    }
}

/// This produces the same hash as [str]
impl Hash for BytesString {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_str().hash(state);
    }
}

impl fmt::Write for BytesString {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.push_str(s);
        Ok(())
    }

    fn write_char(&mut self, c: char) -> fmt::Result {
        self.push(c);
        Ok(())
    }
}

#[cfg(feature = "serde")]
mod serde_impl {
    use serde::{Deserialize, Deserializer, Serialize, Serializer};

    use super::*;

    impl<'de> Deserialize<'de> for BytesString {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            let s = String::deserialize(deserializer)?;
            Ok(Self::from(s))
        }
    }

    impl Serialize for BytesString {
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
        collections::{hash_map::DefaultHasher, HashMap},
        hash::{Hash, Hasher},
    };

    use super::*;

    #[test]
    fn test_new() {
        let s = BytesString::new();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
        assert_eq!(s.as_str(), "");
    }

    #[test]
    fn test_with_capacity() {
        let s = BytesString::with_capacity(10);
        assert!(s.capacity() >= 10);
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
    }

    #[test]
    fn test_from_str() {
        let s = BytesString::from("hello");
        assert_eq!(s.as_str(), "hello");
        assert_eq!(s.len(), 5);
        assert!(!s.is_empty());
    }

    #[test]
    fn test_from_string() {
        let original = String::from("hello world");
        let s = BytesString::from(original);
        assert_eq!(s.as_str(), "hello world");
        assert_eq!(s.len(), 11);
    }

    #[test]
    fn test_from_char() {
        let s = BytesString::from('H');
        assert_eq!(s.as_str(), "H");
        assert_eq!(s.len(), 1);

        // Test unicode character
        let s = BytesString::from('한');
        assert_eq!(s.as_str(), "한");
        assert_eq!(s.len(), 3); // UTF-8 encoding
    }

    #[test]
    fn test_push() {
        let mut s = BytesString::from("hello");
        s.push(' ');
        s.push('w');
        assert_eq!(s.as_str(), "hello w");

        // Test unicode
        s.push('한');
        assert_eq!(s.as_str(), "hello w한");
    }

    #[test]
    fn test_push_str() {
        let mut s = BytesString::from("hello");
        s.push_str(" world");
        assert_eq!(s.as_str(), "hello world");

        s.push_str("!");
        assert_eq!(s.as_str(), "hello world!");

        // Test unicode
        s.push_str(" 한국어");
        assert_eq!(s.as_str(), "hello world! 한국어");
    }

    #[test]
    fn test_clear() {
        let mut s = BytesString::from("hello");
        assert!(!s.is_empty());
        s.clear();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
        assert_eq!(s.as_str(), "");
    }

    #[test]
    fn test_truncate() {
        let mut s = BytesString::from("hello world");
        s.truncate(5);
        assert_eq!(s.as_str(), "hello");

        // Test truncating to larger than current length
        s.truncate(20);
        assert_eq!(s.as_str(), "hello");

        // Test unicode boundaries
        let mut s = BytesString::from("한국어");
        s.truncate(6); // Should truncate to "한국"
        assert_eq!(s.as_str(), "한국");
    }

    #[test]
    #[should_panic]
    fn test_truncate_panic_on_char_boundary() {
        let mut s = BytesString::from("한국어");
        s.truncate(1); // Should panic as it's not on a char boundary
    }

    #[test]
    fn test_split_off() {
        let mut s = BytesString::from("hello world");
        let other = s.split_off(6);
        assert_eq!(s.as_str(), "hello ");
        assert_eq!(other.as_str(), "world");

        // Test at beginning
        let mut s = BytesString::from("hello");
        let other = s.split_off(0);
        assert_eq!(s.as_str(), "");
        assert_eq!(other.as_str(), "hello");

        // Test at end
        let mut s = BytesString::from("hello");
        let other = s.split_off(5);
        assert_eq!(s.as_str(), "hello");
        assert_eq!(other.as_str(), "");
    }

    #[test]
    fn test_as_bytes() {
        let s = BytesString::from("hello");
        assert_eq!(s.as_bytes(), b"hello");

        let s = BytesString::from("한국어");
        assert_eq!(s.as_bytes(), "한국어".as_bytes());
    }

    #[test]
    fn test_as_mut_str() {
        let mut s = BytesString::from("hello");
        s.as_mut_str().make_ascii_uppercase();
        assert_eq!(s.as_str(), "HELLO");
    }

    #[test]
    fn test_into_bytes() {
        let s = BytesString::from("hello");
        let bytes = s.into_bytes();
        assert_eq!(bytes.as_ref(), b"hello");
    }

    #[test]
    fn test_from_utf8() {
        let bytes = BytesMut::from(&b"hello"[..]);
        let s = BytesString::from_utf8(bytes).unwrap();
        assert_eq!(s.as_str(), "hello");

        // Test invalid UTF-8
        let invalid_bytes = BytesMut::from(&[0xff, 0xfe][..]);
        assert!(BytesString::from_utf8(invalid_bytes).is_err());
    }

    #[test]
    fn test_from_utf8_slice() {
        let s = BytesString::from_utf8_slice(b"hello").unwrap();
        assert_eq!(s.as_str(), "hello");

        // Test invalid UTF-8
        assert!(BytesString::from_utf8_slice(&[0xff, 0xfe]).is_err());
    }

    #[test]
    fn test_from_bytes_unchecked() {
        let bytes = BytesMut::from(&b"hello"[..]);
        let s = unsafe { BytesString::from_bytes_unchecked(bytes) };
        assert_eq!(s.as_str(), "hello");
    }

    #[test]
    fn test_reserve() {
        let mut s = BytesString::from("hello");
        let initial_capacity = s.capacity();
        s.reserve(100);
        assert!(s.capacity() >= initial_capacity + 100);
        assert_eq!(s.as_str(), "hello"); // Content unchanged
    }

    #[test]
    fn test_deref() {
        let s = BytesString::from("hello world");
        assert_eq!(s.len(), 11);
        assert!(s.contains("world"));
        assert!(s.starts_with("hello"));
        assert!(s.ends_with("world"));
    }

    #[test]
    fn test_partial_eq() {
        let s = BytesString::from("hello");

        // Test with &str
        assert_eq!(s, "hello");
        assert_ne!(s, "world");

        // Test with String
        assert_eq!(s, String::from("hello"));
        assert_ne!(s, String::from("world"));

        // Test with Cow
        assert_eq!(s, Cow::Borrowed("hello"));
        assert_eq!(s, Cow::Owned(String::from("hello")));

        // Test with Bytes
        assert_eq!(Bytes::from("hello"), s);
        assert_ne!(Bytes::from("world"), s);

        // Test symmetry
        assert_eq!("hello", s);
        assert_eq!(String::from("hello"), s);
    }

    #[test]
    fn test_ordering() {
        let s1 = BytesString::from("apple");
        let s2 = BytesString::from("banana");
        let s3 = BytesString::from("apple");

        assert!(s1 < s2);
        assert!(s2 > s1);
        assert_eq!(s1, s3);
        assert!(s1 <= s3);
        assert!(s1 >= s3);
    }

    #[test]
    fn test_hash() {
        let s1 = BytesString::from("hello");
        let s2 = BytesString::from("hello");
        let s3 = BytesString::from("world");

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
    fn test_add() {
        let s1 = BytesString::from("hello");
        let s2 = s1 + " world";
        assert_eq!(s2.as_str(), "hello world");

        let s3 = BytesString::from("foo");
        let s4 = BytesString::from("bar");
        let s5 = s3 + s4;
        assert_eq!(s5.as_str(), "foobar");
    }

    #[test]
    fn test_add_assign() {
        let mut s = BytesString::from("hello");
        s += " world";
        assert_eq!(s.as_str(), "hello world");

        let mut s1 = BytesString::from("foo");
        let s2 = BytesString::from("bar");
        s1 += s2;
        assert_eq!(s1.as_str(), "foobar");
    }

    #[test]
    fn test_extend_char() {
        let mut s = BytesString::from("hello");
        s.extend(['!', ' ', '🎉'].iter());
        assert_eq!(s.as_str(), "hello! 🎉");

        let mut s = BytesString::new();
        s.extend(['a', 'b', 'c']);
        assert_eq!(s.as_str(), "abc");
    }

    #[test]
    fn test_extend_str() {
        let mut s = BytesString::from("hello");
        s.extend([" ", "world", "!"].iter());
        assert_eq!(s.as_str(), "hello world!");

        let strings = vec![String::from("foo"), String::from("bar")];
        let mut s = BytesString::new();
        s.extend(&strings);
        assert_eq!(s.as_str(), "foobar");
    }

    #[test]
    fn test_extend_bytes_string() {
        let mut s = BytesString::from("hello");
        let parts = vec![BytesString::from(" "), BytesString::from("world")];
        s.extend(parts);
        assert_eq!(s.as_str(), "hello world");
    }

    #[test]
    fn test_from_iterator() {
        let s: BytesString = ['h', 'e', 'l', 'l', 'o'].into_iter().collect();
        assert_eq!(s.as_str(), "hello");

        let s: BytesString = ["hello", " ", "world"].into_iter().collect();
        assert_eq!(s.as_str(), "hello world");

        let strings = vec![String::from("foo"), String::from("bar")];
        let s: BytesString = strings.into_iter().collect();
        assert_eq!(s.as_str(), "foobar");
    }

    #[test]
    fn test_from_str_trait() {
        let s: BytesString = "hello world".parse().unwrap();
        assert_eq!(s.as_str(), "hello world");
    }

    #[test]
    fn test_index() {
        let s = BytesString::from("hello world");
        assert_eq!(&s[0..5], "hello");
        assert_eq!(&s[6..], "world");
        assert_eq!(&s[..5], "hello");
        assert_eq!(&s[6..11], "world");
    }

    #[test]
    fn test_index_mut() {
        let mut s = BytesString::from("hello world");
        s[0..5].make_ascii_uppercase();
        assert_eq!(s.as_str(), "HELLO world");
    }

    #[test]
    fn test_as_ref_implementations() {
        let s = BytesString::from("hello/world");

        // AsRef<str>
        let str_ref: &str = s.as_ref();
        assert_eq!(str_ref, "hello/world");

        // AsRef<[u8]>
        let bytes_ref: &[u8] = s.as_ref();
        assert_eq!(bytes_ref, b"hello/world");

        // AsRef<OsStr>
        let os_str_ref: &OsStr = s.as_ref();
        assert_eq!(os_str_ref, OsStr::new("hello/world"));

        // AsRef<Path>
        let path_ref: &Path = s.as_ref();
        assert_eq!(path_ref, Path::new("hello/world"));
    }

    #[test]
    fn test_borrow() {
        let s = BytesString::from("hello");
        let borrowed: &str = s.borrow();
        assert_eq!(borrowed, "hello");

        let mut s = BytesString::from("hello");
        let borrowed_mut: &mut str = s.borrow_mut();
        borrowed_mut.make_ascii_uppercase();
        assert_eq!(s.as_str(), "HELLO");
    }

    #[test]
    fn test_debug() {
        let s = BytesString::from("hello");
        assert_eq!(format!("{:?}", s), "\"hello\"");
    }

    #[test]
    fn test_display() {
        let s = BytesString::from("hello world");
        assert_eq!(format!("{}", s), "hello world");
    }

    #[test]
    fn test_conversions() {
        let s = BytesString::from("hello");

        // Into BytesMut
        let bytes_mut: BytesMut = s.clone().into();
        assert_eq!(bytes_mut.as_ref(), b"hello");

        // Into Bytes
        let bytes: Bytes = s.into();
        assert_eq!(bytes.as_ref(), b"hello");
    }

    #[test]
    fn test_to_socket_addrs() {
        let s = BytesString::from("127.0.0.1:8080");
        let addrs: Vec<_> = s.to_socket_addrs().unwrap().collect();
        assert!(!addrs.is_empty());

        let s = BytesString::from("localhost:8080");
        let result = s.to_socket_addrs();
        // This might fail depending on system configuration, so we just check it
        // compiles
        let _ = result;
    }

    #[test]
    fn test_unicode_handling() {
        let s = BytesString::from("Hello 🌍 한국어 🎉");
        assert_eq!(s.as_str(), "Hello 🌍 한국어 🎉");
        assert!(s.len() > 13); // More than ASCII length due to UTF-8 encoding

        let mut s = BytesString::new();
        s.push('🌍');
        s.push_str(" 한국어");
        assert_eq!(s.as_str(), "🌍 한국어");
    }

    #[test]
    fn test_empty_operations() {
        let mut s = BytesString::new();
        assert!(s.is_empty());

        s.push_str("");
        assert!(s.is_empty());

        s.clear();
        assert!(s.is_empty());

        let other = s.split_off(0);
        assert!(s.is_empty());
        assert!(other.is_empty());
    }

    #[test]
    fn test_large_string() {
        let large_str = "a".repeat(10000);
        let s = BytesString::from(large_str.as_str());
        assert_eq!(s.len(), 10000);
        assert_eq!(s.as_str(), large_str);

        let mut s = BytesString::with_capacity(10000);
        for _ in 0..10000 {
            s.push('a');
        }
        assert_eq!(s.len(), 10000);
        assert_eq!(s.as_str(), large_str);
    }

    #[test]
    fn test_clone() {
        let s1 = BytesString::from("hello world");
        let s2 = s1.clone();
        assert_eq!(s1, s2);
        assert_eq!(s1.as_str(), s2.as_str());
    }

    #[test]
    fn test_default() {
        let s: BytesString = Default::default();
        assert!(s.is_empty());
        assert_eq!(s.as_str(), "");
    }

    #[test]
    fn test_hash_map_usage() {
        let mut map = HashMap::new();
        let key = BytesString::from("key");
        map.insert(key, "value");

        let lookup_key = BytesString::from("key");
        assert_eq!(map.get(&lookup_key), Some(&"value"));

        // Test that string can be used to lookup BytesString key
        assert_eq!(map.get("key"), Some(&"value"));
    }
}
