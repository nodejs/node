//! Custom Content enum implementation for AST node deserialization.
//! This replaces the use of serde's private Content enum which is not stable
//! API.

use std::{fmt, marker::PhantomData};

use serde::{
    de::{self, IntoDeserializer, VariantAccess},
    Deserializer,
};

/// Content holds a buffered representation of any serde data type.
/// This is used for deserializing tagged enums where we need to buffer
/// the deserializer content and then deserialize from the buffer.
#[derive(Debug, Clone)]
pub enum Content<'de> {
    Bool(bool),
    U8(u8),
    U16(u16),
    U32(u32),
    U64(u64),
    I8(i8),
    I16(i16),
    I32(i32),
    I64(i64),
    F32(f32),
    F64(f64),
    Char(char),
    String(String),
    Str(&'de str),
    ByteBuf(Vec<u8>),
    Bytes(&'de [u8]),
    None,
    Some(Box<Content<'de>>),
    Unit,
    Newtype(Box<Content<'de>>),
    Seq(Vec<Content<'de>>),
    Map(Vec<(Content<'de>, Content<'de>)>),
}

impl<'de> Content<'de> {
    /// Convert this content into a string if possible.
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Content::Str(s) => Some(s),
            Content::String(s) => Some(s),
            _ => None,
        }
    }

    /// Create an Unexpected value for error reporting.
    pub fn unexpected(&self) -> de::Unexpected {
        match self {
            Content::Bool(b) => de::Unexpected::Bool(*b),
            Content::U8(n) => de::Unexpected::Unsigned(*n as u64),
            Content::U16(n) => de::Unexpected::Unsigned(*n as u64),
            Content::U32(n) => de::Unexpected::Unsigned(*n as u64),
            Content::U64(n) => de::Unexpected::Unsigned(*n),
            Content::I8(n) => de::Unexpected::Signed(*n as i64),
            Content::I16(n) => de::Unexpected::Signed(*n as i64),
            Content::I32(n) => de::Unexpected::Signed(*n as i64),
            Content::I64(n) => de::Unexpected::Signed(*n),
            Content::F32(f) => de::Unexpected::Float(*f as f64),
            Content::F64(f) => de::Unexpected::Float(*f),
            Content::Char(c) => de::Unexpected::Char(*c),
            Content::String(s) => de::Unexpected::Str(s),
            Content::Str(s) => de::Unexpected::Str(s),
            Content::ByteBuf(b) => de::Unexpected::Bytes(b),
            Content::Bytes(b) => de::Unexpected::Bytes(b),
            Content::None | Content::Unit => de::Unexpected::Unit,
            Content::Some(_) => de::Unexpected::Option,
            Content::Newtype(_) => de::Unexpected::NewtypeStruct,
            Content::Seq(_) => de::Unexpected::Seq,
            Content::Map(_) => de::Unexpected::Map,
        }
    }
}

/// A visitor for deserializing `Content`.
#[derive(Default)]
pub struct ContentVisitor<'de> {
    marker: PhantomData<Content<'de>>,
}

impl<'de> ContentVisitor<'de> {
    pub fn new() -> Self {
        ContentVisitor::default()
    }
}

impl<'de> de::Visitor<'de> for ContentVisitor<'de> {
    type Value = Content<'de>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("any value")
    }

    fn visit_bool<E>(self, value: bool) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::Bool(value))
    }

    fn visit_i8<E>(self, value: i8) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::I8(value))
    }

    fn visit_i16<E>(self, value: i16) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::I16(value))
    }

    fn visit_i32<E>(self, value: i32) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::I32(value))
    }

    fn visit_i64<E>(self, value: i64) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::I64(value))
    }

    fn visit_u8<E>(self, value: u8) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::U8(value))
    }

    fn visit_u16<E>(self, value: u16) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::U16(value))
    }

    fn visit_u32<E>(self, value: u32) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::U32(value))
    }

    fn visit_u64<E>(self, value: u64) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::U64(value))
    }

    fn visit_f32<E>(self, value: f32) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::F32(value))
    }

    fn visit_f64<E>(self, value: f64) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::F64(value))
    }

    fn visit_char<E>(self, value: char) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::Char(value))
    }

    fn visit_str<E>(self, value: &str) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::String(value.to_owned()))
    }

    fn visit_borrowed_str<E>(self, value: &'de str) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::Str(value))
    }

    fn visit_string<E>(self, value: String) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::String(value))
    }

    fn visit_bytes<E>(self, value: &[u8]) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::ByteBuf(value.to_vec()))
    }

    fn visit_borrowed_bytes<E>(self, value: &'de [u8]) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::Bytes(value))
    }

    fn visit_byte_buf<E>(self, value: Vec<u8>) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::ByteBuf(value))
    }

    fn visit_none<E>(self) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::None)
    }

    fn visit_some<D>(self, deserializer: D) -> Result<Content<'de>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let content = deserializer.deserialize_any(ContentVisitor::new())?;
        Ok(Content::Some(Box::new(content)))
    }

    fn visit_unit<E>(self) -> Result<Content<'de>, E>
    where
        E: de::Error,
    {
        Ok(Content::Unit)
    }

    fn visit_newtype_struct<D>(self, deserializer: D) -> Result<Content<'de>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let content = deserializer.deserialize_any(ContentVisitor::new())?;
        Ok(Content::Newtype(Box::new(content)))
    }

    fn visit_seq<V>(self, mut seq: V) -> Result<Content<'de>, V::Error>
    where
        V: de::SeqAccess<'de>,
    {
        let mut vec = Vec::with_capacity(seq.size_hint().unwrap_or(0));
        while let Some(elem) = seq.next_element_seed(ContentSeed::new())? {
            vec.push(elem);
        }
        Ok(Content::Seq(vec))
    }

    fn visit_map<V>(self, mut map: V) -> Result<Content<'de>, V::Error>
    where
        V: de::MapAccess<'de>,
    {
        let mut vec = Vec::with_capacity(map.size_hint().unwrap_or(0));
        while let Some((key, value)) =
            map.next_entry_seed(ContentSeed::new(), ContentSeed::new())?
        {
            vec.push((key, value));
        }
        Ok(Content::Map(vec))
    }

    fn visit_enum<V>(self, data: V) -> Result<Content<'de>, V::Error>
    where
        V: de::EnumAccess<'de>,
    {
        let ((), variant_access) = data.variant::<()>()?;
        let content = variant_access.newtype_variant_seed(ContentSeed::new())?;
        Ok(Content::Newtype(Box::new(content)))
    }
}

/// Helper for deserializing Content in sequences and maps.
struct ContentSeed<'de> {
    marker: PhantomData<Content<'de>>,
}

impl<'de> ContentSeed<'de> {
    fn new() -> Self {
        ContentSeed {
            marker: PhantomData,
        }
    }
}

impl<'de> de::DeserializeSeed<'de> for ContentSeed<'de> {
    type Value = Content<'de>;

    fn deserialize<D>(self, deserializer: D) -> Result<Content<'de>, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(ContentVisitor::new())
    }
}

/// Deserialize an instance of type `T` from a `Content`.
pub struct ContentDeserializer<'de, E> {
    content: Content<'de>,
    marker: PhantomData<E>,
}

impl<'de, E> ContentDeserializer<'de, E> {
    pub fn new(content: Content<'de>) -> Self {
        ContentDeserializer {
            content,
            marker: PhantomData,
        }
    }
}

impl<'de, E> Deserializer<'de> for ContentDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Bool(v) => visitor.visit_bool(v),
            Content::U8(v) => visitor.visit_u8(v),
            Content::U16(v) => visitor.visit_u16(v),
            Content::U32(v) => visitor.visit_u32(v),
            Content::U64(v) => visitor.visit_u64(v),
            Content::I8(v) => visitor.visit_i8(v),
            Content::I16(v) => visitor.visit_i16(v),
            Content::I32(v) => visitor.visit_i32(v),
            Content::I64(v) => visitor.visit_i64(v),
            Content::F32(v) => visitor.visit_f32(v),
            Content::F64(v) => visitor.visit_f64(v),
            Content::Char(v) => visitor.visit_char(v),
            Content::String(v) => visitor.visit_string(v),
            Content::Str(v) => visitor.visit_borrowed_str(v),
            Content::ByteBuf(v) => visitor.visit_byte_buf(v),
            Content::Bytes(v) => visitor.visit_borrowed_bytes(v),
            Content::None => visitor.visit_none(),
            Content::Some(v) => visitor.visit_some(ContentDeserializer::new(*v)),
            Content::Unit => visitor.visit_unit(),
            Content::Newtype(v) => visitor.visit_newtype_struct(ContentDeserializer::new(*v)),
            Content::Seq(v) => {
                let seq = SeqDeserializer::new(v.into_iter());
                visitor.visit_seq(seq)
            }
            Content::Map(v) => {
                let map = MapDeserializer::new(v.into_iter());
                visitor.visit_map(map)
            }
        }
    }

    fn deserialize_bool<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Bool(v) => visitor.visit_bool(v),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_i8<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i64(visitor)
    }

    fn deserialize_i16<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i64(visitor)
    }

    fn deserialize_i32<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i64(visitor)
    }

    fn deserialize_i64<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::I8(v) => visitor.visit_i64(v as i64),
            Content::I16(v) => visitor.visit_i64(v as i64),
            Content::I32(v) => visitor.visit_i64(v as i64),
            Content::I64(v) => visitor.visit_i64(v),
            Content::U8(v) => visitor.visit_i64(v as i64),
            Content::U16(v) => visitor.visit_i64(v as i64),
            Content::U32(v) => visitor.visit_i64(v as i64),
            Content::U64(v) => {
                if v <= i64::MAX as u64 {
                    visitor.visit_i64(v as i64)
                } else {
                    Err(de::Error::invalid_type(self.content.unexpected(), &visitor))
                }
            }
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_u8<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u64(visitor)
    }

    fn deserialize_u16<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u64(visitor)
    }

    fn deserialize_u32<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u64(visitor)
    }

    fn deserialize_u64<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::U8(v) => visitor.visit_u64(v as u64),
            Content::U16(v) => visitor.visit_u64(v as u64),
            Content::U32(v) => visitor.visit_u64(v as u64),
            Content::U64(v) => visitor.visit_u64(v),
            Content::I8(v) if v >= 0 => visitor.visit_u64(v as u64),
            Content::I16(v) if v >= 0 => visitor.visit_u64(v as u64),
            Content::I32(v) if v >= 0 => visitor.visit_u64(v as u64),
            Content::I64(v) if v >= 0 => visitor.visit_u64(v as u64),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_f32<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_f64(visitor)
    }

    fn deserialize_f64<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::F32(v) => visitor.visit_f64(v as f64),
            Content::F64(v) => visitor.visit_f64(v),
            Content::I8(v) => visitor.visit_f64(v as f64),
            Content::I16(v) => visitor.visit_f64(v as f64),
            Content::I32(v) => visitor.visit_f64(v as f64),
            Content::I64(v) => visitor.visit_f64(v as f64),
            Content::U8(v) => visitor.visit_f64(v as f64),
            Content::U16(v) => visitor.visit_f64(v as f64),
            Content::U32(v) => visitor.visit_f64(v as f64),
            Content::U64(v) => visitor.visit_f64(v as f64),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_char<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Char(v) => visitor.visit_char(v),
            Content::String(v) => {
                let mut chars = v.chars();
                if let Some(c) = chars.next() {
                    if chars.next().is_none() {
                        visitor.visit_char(c)
                    } else {
                        Err(de::Error::invalid_length(v.len(), &visitor))
                    }
                } else {
                    Err(de::Error::invalid_length(0, &visitor))
                }
            }
            Content::Str(v) => {
                let mut chars = v.chars();
                if let Some(c) = chars.next() {
                    if chars.next().is_none() {
                        visitor.visit_char(c)
                    } else {
                        Err(de::Error::invalid_length(v.len(), &visitor))
                    }
                } else {
                    Err(de::Error::invalid_length(0, &visitor))
                }
            }
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_string(visitor)
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::String(v) => visitor.visit_string(v),
            Content::Str(v) => visitor.visit_borrowed_str(v),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_bytes<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_byte_buf(visitor)
    }

    fn deserialize_byte_buf<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::ByteBuf(v) => visitor.visit_byte_buf(v),
            Content::Bytes(v) => visitor.visit_borrowed_bytes(v),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::None => visitor.visit_none(),
            Content::Some(v) => visitor.visit_some(ContentDeserializer::new(*v)),
            Content::Unit => visitor.visit_unit(),
            _ => visitor.visit_some(self),
        }
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Unit => visitor.visit_unit(),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_unit_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_unit(visitor)
    }

    fn deserialize_newtype_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Newtype(v) => visitor.visit_newtype_struct(ContentDeserializer::new(*v)),
            _ => visitor.visit_newtype_struct(self),
        }
    }

    fn deserialize_seq<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Seq(v) => {
                let seq = SeqDeserializer::new(v.into_iter());
                visitor.visit_seq(seq)
            }
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_tuple<V>(self, _len: usize, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_seq(visitor)
    }

    fn deserialize_tuple_struct<V>(
        self,
        _name: &'static str,
        _len: usize,
        visitor: V,
    ) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_seq(visitor)
    }

    fn deserialize_map<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Map(v) => {
                let map = MapDeserializer::new(v.into_iter());
                visitor.visit_map(map)
            }
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_struct<V>(
        self,
        _name: &'static str,
        _fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Map(v) => {
                let map = MapDeserializer::new(v.into_iter());
                visitor.visit_map(map)
            }
            _ => self.deserialize_any(visitor),
        }
    }

    fn deserialize_enum<V>(
        self,
        _name: &'static str,
        _variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::String(s) => visitor.visit_enum(StrDeserializer::new(s)),
            Content::Str(s) => visitor.visit_enum(BorrowedStrDeserializer::new(s)),
            _ => Err(de::Error::invalid_type(self.content.unexpected(), &visitor)),
        }
    }

    fn deserialize_identifier<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_string(visitor)
    }

    fn deserialize_ignored_any<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_unit()
    }

    fn is_human_readable(&self) -> bool {
        false
    }
}

/// Deserialize a reference to the content.
pub struct ContentRefDeserializer<'de, E> {
    content: &'de Content<'de>,
    marker: PhantomData<E>,
}

impl<'de, E> ContentRefDeserializer<'de, E> {
    pub fn new(content: &'de Content<'de>) -> Self {
        ContentRefDeserializer {
            content,
            marker: PhantomData,
        }
    }
}

impl<'de, E> Deserializer<'de> for ContentRefDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;

    // Forward other methods to deserialize_any
    serde::forward_to_deserialize_any! {
        bool i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 char str string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        match self.content {
            Content::Bool(v) => visitor.visit_bool(*v),
            Content::U8(v) => visitor.visit_u8(*v),
            Content::U16(v) => visitor.visit_u16(*v),
            Content::U32(v) => visitor.visit_u32(*v),
            Content::U64(v) => visitor.visit_u64(*v),
            Content::I8(v) => visitor.visit_i8(*v),
            Content::I16(v) => visitor.visit_i16(*v),
            Content::I32(v) => visitor.visit_i32(*v),
            Content::I64(v) => visitor.visit_i64(*v),
            Content::F32(v) => visitor.visit_f32(*v),
            Content::F64(v) => visitor.visit_f64(*v),
            Content::Char(v) => visitor.visit_char(*v),
            Content::String(v) => visitor.visit_str(v),
            Content::Str(v) => visitor.visit_borrowed_str(v),
            Content::ByteBuf(v) => visitor.visit_bytes(v),
            Content::Bytes(v) => visitor.visit_borrowed_bytes(v),
            Content::None => visitor.visit_none(),
            Content::Some(v) => visitor.visit_some(ContentRefDeserializer::new(v)),
            Content::Unit => visitor.visit_unit(),
            Content::Newtype(v) => visitor.visit_newtype_struct(ContentRefDeserializer::new(v)),
            Content::Seq(v) => {
                let seq = SeqRefDeserializer::new(v.iter());
                visitor.visit_seq(seq)
            }
            Content::Map(v) => {
                let map = MapRefDeserializer::new(v.iter());
                visitor.visit_map(map)
            }
        }
    }

    fn is_human_readable(&self) -> bool {
        false
    }
}

// Helper deserializers for sequences and maps

struct SeqDeserializer<'de, E> {
    iter: std::vec::IntoIter<Content<'de>>,
    marker: PhantomData<E>,
}

impl<'de, E> SeqDeserializer<'de, E> {
    fn new(iter: std::vec::IntoIter<Content<'de>>) -> Self {
        SeqDeserializer {
            iter,
            marker: PhantomData,
        }
    }
}

impl<'de, E> de::SeqAccess<'de> for SeqDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, E>
    where
        T: de::DeserializeSeed<'de>,
    {
        match self.iter.next() {
            Some(value) => seed.deserialize(ContentDeserializer::new(value)).map(Some),
            None => Ok(None),
        }
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.iter.len())
    }
}

struct MapDeserializer<'de, E> {
    iter: std::vec::IntoIter<(Content<'de>, Content<'de>)>,
    value: Option<Content<'de>>,
    marker: PhantomData<E>,
}

impl<'de, E> MapDeserializer<'de, E> {
    fn new(iter: std::vec::IntoIter<(Content<'de>, Content<'de>)>) -> Self {
        MapDeserializer {
            iter,
            value: None,
            marker: PhantomData,
        }
    }
}

impl<'de, E> de::MapAccess<'de> for MapDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, E>
    where
        K: de::DeserializeSeed<'de>,
    {
        match self.iter.next() {
            Some((key, value)) => {
                self.value = Some(value);
                seed.deserialize(ContentDeserializer::new(key)).map(Some)
            }
            None => Ok(None),
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, E>
    where
        V: de::DeserializeSeed<'de>,
    {
        match self.value.take() {
            Some(value) => seed.deserialize(ContentDeserializer::new(value)),
            None => Err(de::Error::custom("value is missing")),
        }
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.iter.len())
    }
}

struct SeqRefDeserializer<'de, E> {
    iter: std::slice::Iter<'de, Content<'de>>,
    marker: PhantomData<E>,
}

impl<'de, E> SeqRefDeserializer<'de, E> {
    fn new(iter: std::slice::Iter<'de, Content<'de>>) -> Self {
        SeqRefDeserializer {
            iter,
            marker: PhantomData,
        }
    }
}

impl<'de, E> de::SeqAccess<'de> for SeqRefDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, E>
    where
        T: de::DeserializeSeed<'de>,
    {
        match self.iter.next() {
            Some(value) => seed
                .deserialize(ContentRefDeserializer::new(value))
                .map(Some),
            None => Ok(None),
        }
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.iter.len())
    }
}

struct MapRefDeserializer<'de, E> {
    iter: std::slice::Iter<'de, (Content<'de>, Content<'de>)>,
    value: Option<&'de Content<'de>>,
    marker: PhantomData<E>,
}

impl<'de, E> MapRefDeserializer<'de, E> {
    fn new(iter: std::slice::Iter<'de, (Content<'de>, Content<'de>)>) -> Self {
        MapRefDeserializer {
            iter,
            value: None,
            marker: PhantomData,
        }
    }
}

impl<'de, E> de::MapAccess<'de> for MapRefDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, E>
    where
        K: de::DeserializeSeed<'de>,
    {
        match self.iter.next() {
            Some((key, value)) => {
                self.value = Some(value);
                seed.deserialize(ContentRefDeserializer::new(key)).map(Some)
            }
            None => Ok(None),
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, E>
    where
        V: de::DeserializeSeed<'de>,
    {
        match self.value.take() {
            Some(value) => seed.deserialize(ContentRefDeserializer::new(value)),
            None => Err(de::Error::custom("value is missing")),
        }
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.iter.len())
    }
}

struct StrDeserializer<E> {
    value: String,
    marker: PhantomData<E>,
}

impl<E> StrDeserializer<E> {
    fn new(value: String) -> Self {
        StrDeserializer {
            value,
            marker: PhantomData,
        }
    }
}

impl<'de, E> de::EnumAccess<'de> for StrDeserializer<E>
where
    E: de::Error,
{
    type Error = E;
    type Variant = UnitVariant<E>;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self::Variant), E>
    where
        V: de::DeserializeSeed<'de>,
    {
        let variant = seed.deserialize(self.value.into_deserializer())?;
        Ok((
            variant,
            UnitVariant {
                marker: PhantomData,
            },
        ))
    }
}

struct BorrowedStrDeserializer<'de, E> {
    value: &'de str,
    marker: PhantomData<E>,
}

impl<'de, E> BorrowedStrDeserializer<'de, E> {
    fn new(value: &'de str) -> Self {
        BorrowedStrDeserializer {
            value,
            marker: PhantomData,
        }
    }
}

impl<'de, E> de::EnumAccess<'de> for BorrowedStrDeserializer<'de, E>
where
    E: de::Error,
{
    type Error = E;
    type Variant = UnitVariant<E>;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self::Variant), E>
    where
        V: de::DeserializeSeed<'de>,
    {
        let variant = seed.deserialize(self.value.into_deserializer())?;
        Ok((
            variant,
            UnitVariant {
                marker: PhantomData,
            },
        ))
    }
}

struct UnitVariant<E> {
    marker: PhantomData<E>,
}

impl<'de, E> de::VariantAccess<'de> for UnitVariant<E>
where
    E: de::Error,
{
    type Error = E;

    fn unit_variant(self) -> Result<(), E> {
        Ok(())
    }

    fn newtype_variant_seed<T>(self, _seed: T) -> Result<T::Value, E>
    where
        T: de::DeserializeSeed<'de>,
    {
        Err(de::Error::invalid_type(
            de::Unexpected::UnitVariant,
            &"newtype variant",
        ))
    }

    fn tuple_variant<V>(self, _len: usize, _visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        Err(de::Error::invalid_type(
            de::Unexpected::UnitVariant,
            &"tuple variant",
        ))
    }

    fn struct_variant<V>(self, _fields: &'static [&'static str], _visitor: V) -> Result<V::Value, E>
    where
        V: de::Visitor<'de>,
    {
        Err(de::Error::invalid_type(
            de::Unexpected::UnitVariant,
            &"struct variant",
        ))
    }
}

// Helper function to deserialize content
pub fn deserialize_content<'de, D>(deserializer: D) -> Result<Content<'de>, D::Error>
where
    D: Deserializer<'de>,
{
    deserializer.deserialize_any(ContentVisitor::new())
}
