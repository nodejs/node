#![allow(unexpected_cfgs)]

use std::fmt::{Debug, Display};

use string_enum::*;

pub trait Assert: Debug + Display {}

#[derive(StringEnum)]
pub enum Tokens {
    ///`a`
    #[string_enum(alias("foo"))]
    A,
    /// `b`
    B,
}

impl Assert for Tokens {}

#[test]
fn as_str() {
    assert_eq!(Tokens::A.as_str(), "a");
    assert_eq!(Tokens::B.as_str(), "b");
}

#[test]
fn test_fmt() {
    assert_eq!(Tokens::A.to_string(), "a");
    assert_eq!(format!("{}", Tokens::A), "a");
}
