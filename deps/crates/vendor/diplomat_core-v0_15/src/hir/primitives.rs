//! Primitives types that can cross the FFI boundary.
use crate::ast;

/// 8, 16, 32, and 64-bit signed and unsigned integers.
#[derive(Copy, Clone, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only these
pub enum IntType {
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
}

/// Platform-dependent signed and unsigned size types.
#[derive(Copy, Clone, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only these
pub enum IntSizeType {
    Isize,
    Usize,
}

/// 128-bit signed and unsigned integers.
#[derive(Copy, Clone, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only these
pub enum Int128Type {
    I128,
    U128,
}

/// 32 and 64-bit floating point numbers.
#[derive(Copy, Clone, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only these
pub enum FloatType {
    F32,
    F64,
}

/// All primitive types.
#[derive(Copy, Clone, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only these
pub enum PrimitiveType {
    Bool,
    Char,
    /// a primitive byte that is not meant to be interpreted numerically
    /// in languages that don't have fine-grained integer types
    Byte,
    /// an i8 that is either -1, 0, or 1
    Ordering,
    Int(IntType),
    IntSize(IntSizeType),
    Int128(Int128Type),
    Float(FloatType),
}

impl IntType {
    /// Returns the string representation of `self`.
    pub fn as_str(&self) -> &'static str {
        match self {
            IntType::I8 => "i8",
            IntType::I16 => "i16",
            IntType::I32 => "i32",
            IntType::I64 => "i64",
            IntType::U8 => "u8",
            IntType::U16 => "u16",
            IntType::U32 => "u32",
            IntType::U64 => "u64",
        }
    }
}

impl IntSizeType {
    /// Returns the string representation of `self`.
    pub fn as_str(&self) -> &'static str {
        match self {
            IntSizeType::Isize => "isize",
            IntSizeType::Usize => "usize",
        }
    }
}

impl Int128Type {
    /// Returns the string representation of `self`.
    pub fn as_str(&self) -> &'static str {
        match self {
            Int128Type::I128 => "i128",
            Int128Type::U128 => "u128",
        }
    }
}

impl FloatType {
    /// Returns the string representation of `self`.
    pub fn as_str(&self) -> &'static str {
        match self {
            FloatType::F32 => "f32",
            FloatType::F64 => "f64",
        }
    }
}

impl PrimitiveType {
    pub(super) fn from_ast(prim: ast::PrimitiveType) -> Self {
        match prim {
            ast::PrimitiveType::i8 => PrimitiveType::Int(IntType::I8),
            ast::PrimitiveType::u8 => PrimitiveType::Int(IntType::U8),
            ast::PrimitiveType::i16 => PrimitiveType::Int(IntType::I16),
            ast::PrimitiveType::u16 => PrimitiveType::Int(IntType::U16),
            ast::PrimitiveType::i32 => PrimitiveType::Int(IntType::I32),
            ast::PrimitiveType::u32 => PrimitiveType::Int(IntType::U32),
            ast::PrimitiveType::i64 => PrimitiveType::Int(IntType::I64),
            ast::PrimitiveType::u64 => PrimitiveType::Int(IntType::U64),
            ast::PrimitiveType::isize => PrimitiveType::IntSize(IntSizeType::Isize),
            ast::PrimitiveType::usize => PrimitiveType::IntSize(IntSizeType::Usize),
            ast::PrimitiveType::i128 => PrimitiveType::Int128(Int128Type::I128),
            ast::PrimitiveType::u128 => PrimitiveType::Int128(Int128Type::U128),
            ast::PrimitiveType::f32 => PrimitiveType::Float(FloatType::F32),
            ast::PrimitiveType::f64 => PrimitiveType::Float(FloatType::F64),
            ast::PrimitiveType::bool => PrimitiveType::Bool,
            ast::PrimitiveType::char => PrimitiveType::Char,
            ast::PrimitiveType::byte => PrimitiveType::Byte,
        }
    }

    /// Returns the string representation of `self`.
    pub fn as_str(&self) -> &'static str {
        match self {
            PrimitiveType::Bool => "bool",
            PrimitiveType::Char => "char",
            PrimitiveType::Byte => "byte",
            PrimitiveType::Ordering => "ordering",
            PrimitiveType::Int(ty) => ty.as_str(),
            PrimitiveType::IntSize(ty) => ty.as_str(),
            PrimitiveType::Int128(ty) => ty.as_str(),
            PrimitiveType::Float(ty) => ty.as_str(),
        }
    }
}
