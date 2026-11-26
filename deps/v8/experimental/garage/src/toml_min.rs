// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Minimal, zero-dependency TOML parser covering the subset `garage` uses:
//! comments, `[table]` / `[a."b"]` section headers, key-value pairs with
//! string, integer, boolean, and array values.

use std::collections::BTreeMap;
use std::fmt;
use std::str::FromStr;

pub type Table = BTreeMap<String, Value>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Value {
    String(String),
    Integer(i64),
    Boolean(bool),
    Array(Vec<Value>),
    Table(Table),
}

impl Value {
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s),
            _ => None,
        }
    }

    pub fn as_array(&self) -> Option<&[Value]> {
        match self {
            Value::Array(a) => Some(a),
            _ => None,
        }
    }

    pub fn get(&self, key: &str) -> Option<&Value> {
        match self {
            Value::Table(t) => t.get(key),
            _ => None,
        }
    }
}

pub mod de {
    use super::Error as ParseError;
    pub type Error = ParseError;
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    line: usize,
    message: String,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "line {}: {}", self.line, self.message)
    }
}

impl std::error::Error for Error {}

pub fn parse_table(input: &str) -> Result<Table, Error> {
    Parser::new(input).parse_root()
}

impl FromStr for Value {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Value::Table(parse_table(s)?))
    }
}

struct Parser<'a> {
    bytes: &'a [u8],
    src: &'a str,
    pos: usize,
    line: usize,
}

impl<'a> Parser<'a> {
    fn new(src: &'a str) -> Self {
        Self {
            bytes: src.as_bytes(),
            src,
            pos: 0,
            line: 1,
        }
    }

    fn err<T>(&self, msg: impl Into<String>) -> Result<T, Error> {
        Err(Error {
            line: self.line,
            message: msg.into(),
        })
    }

    fn peek(&self) -> Option<u8> {
        self.bytes.get(self.pos).copied()
    }

    fn bump(&mut self) -> Option<u8> {
        let b = self.peek()?;
        self.pos += 1;
        if b == b'\n' {
            self.line += 1;
        }
        Some(b)
    }

    fn skip_ws_inline(&mut self) {
        while matches!(self.peek(), Some(b' ' | b'\t' | b'\r')) {
            self.pos += 1;
        }
    }

    fn skip_ws_and_comments(&mut self) -> Result<(), Error> {
        loop {
            while let Some(b) = self.peek() {
                if matches!(b, b' ' | b'\t' | b'\r' | b'\n') {
                    self.bump();
                } else {
                    break;
                }
            }
            if self.peek() == Some(b'#') {
                while let Some(b) = self.peek() {
                    if b == b'\n' {
                        break;
                    }
                    // Reject bare control chars other than tab inside comments per TOML.
                    if b < 0x20 && b != b'\t' && b != b'\r' {
                        return self.err("invalid control character in comment");
                    }
                    self.pos += 1;
                }
            } else {
                break;
            }
        }
        Ok(())
    }

    fn expect_line_end(&mut self) -> Result<(), Error> {
        self.skip_ws_inline();
        if self.peek() == Some(b'#') {
            while let Some(b) = self.peek() {
                if b == b'\n' {
                    break;
                }
                self.pos += 1;
            }
        }
        match self.peek() {
            None => Ok(()),
            Some(b'\n') => {
                self.bump();
                Ok(())
            }
            Some(b) => self.err(format!("unexpected character {:?}", b as char)),
        }
    }

    fn parse_root(&mut self) -> Result<Table, Error> {
        // Strip UTF-8 BOM if present.
        if self.src.starts_with('\u{feff}') {
            self.pos = '\u{feff}'.len_utf8();
        }
        let mut root = Table::new();
        let mut current_path: Vec<String> = Vec::new();

        loop {
            self.skip_ws_and_comments()?;
            let Some(b) = self.peek() else { break };
            if b == b'[' {
                self.bump();
                if self.peek() == Some(b'[') {
                    return self.err("array-of-tables `[[...]]` is not supported");
                }
                let path = self.parse_key_path()?;
                self.skip_ws_inline();
                if self.bump() != Some(b']') {
                    return self.err("unterminated table header, expected `]`");
                }
                self.expect_line_end()?;
                // Ensure the path exists and resolves to a Table.
                let _ = descend_mut(&mut root, &path, self.line)?;
                current_path = path;
            } else {
                let key_path = self.parse_key_path()?;
                self.skip_ws_inline();
                if self.bump() != Some(b'=') {
                    return self.err("expected `=` after key");
                }
                self.skip_ws_inline();
                let value = self.parse_value()?;
                self.expect_line_end()?;

                let target = descend_mut(&mut root, &current_path, self.line)?;
                let (parents, leaf) = key_path.split_at(key_path.len() - 1);
                let leaf_table = descend_mut(target, parents, self.line)?;
                if leaf_table.contains_key(&leaf[0]) {
                    return self.err(format!("duplicate key {:?}", leaf[0]));
                }
                leaf_table.insert(leaf[0].clone(), value);
            }
        }
        Ok(root)
    }

    fn parse_key_path(&mut self) -> Result<Vec<String>, Error> {
        let mut parts = Vec::new();
        loop {
            self.skip_ws_inline();
            parts.push(self.parse_single_key()?);
            self.skip_ws_inline();
            if self.peek() == Some(b'.') {
                self.bump();
            } else {
                break;
            }
        }
        Ok(parts)
    }

    fn parse_single_key(&mut self) -> Result<String, Error> {
        match self.peek() {
            Some(b'"') => self.parse_basic_string(),
            Some(b'\'') => self.parse_literal_string(),
            Some(b) if is_bare_key_byte(b) => {
                let start = self.pos;
                while let Some(c) = self.peek() {
                    if is_bare_key_byte(c) {
                        self.pos += 1;
                    } else {
                        break;
                    }
                }
                Ok(self.src[start..self.pos].to_string())
            }
            _ => self.err("expected key"),
        }
    }

    fn parse_value(&mut self) -> Result<Value, Error> {
        match self.peek() {
            Some(b'"') => self.parse_basic_string().map(Value::String),
            Some(b'\'') => self.parse_literal_string().map(Value::String),
            Some(b'[') => self.parse_array(),
            Some(b'{') => self.parse_inline_table(),
            Some(b't' | b'f') => self.parse_bool(),
            Some(b'+' | b'-' | b'0'..=b'9') => self.parse_integer(),
            _ => self.err("expected value"),
        }
    }

    fn parse_basic_string(&mut self) -> Result<String, Error> {
        self.bump(); // opening `"`
        let mut out = String::new();
        loop {
            match self.peek() {
                None | Some(b'\n') => return self.err("unterminated string"),
                Some(b'"') => {
                    self.pos += 1;
                    return Ok(out);
                }
                Some(b'\\') => {
                    self.pos += 1;
                    let Some(esc) = self.bump() else {
                        return self.err("unterminated escape sequence");
                    };
                    match esc {
                        b'"' => out.push('"'),
                        b'\\' => out.push('\\'),
                        b'b' => out.push('\u{0008}'),
                        b'f' => out.push('\u{000c}'),
                        b'n' => out.push('\n'),
                        b'r' => out.push('\r'),
                        b't' => out.push('\t'),
                        b'u' => out.push(self.parse_unicode_escape(4)?),
                        b'U' => out.push(self.parse_unicode_escape(8)?),
                        other => {
                            return self.err(format!("invalid string escape \\{}", other as char));
                        }
                    }
                }
                Some(_) => {
                    let ch = self.src[self.pos..].chars().next().unwrap();
                    self.pos += ch.len_utf8();
                    out.push(ch);
                }
            }
        }
    }

    fn parse_unicode_escape(&mut self, digits: usize) -> Result<char, Error> {
        if self.pos + digits > self.bytes.len() {
            return self.err("incomplete unicode escape");
        }
        let hex = &self.src[self.pos..self.pos + digits];
        if !hex.bytes().all(|b| b.is_ascii_hexdigit()) {
            return self.err("invalid hex in unicode escape");
        }
        self.pos += digits;
        let code = u32::from_str_radix(hex, 16).map_err(|_| Error {
            line: self.line,
            message: "invalid unicode escape".to_string(),
        })?;
        char::from_u32(code).ok_or_else(|| Error {
            line: self.line,
            message: "unicode escape is not a scalar value".to_string(),
        })
    }

    fn parse_literal_string(&mut self) -> Result<String, Error> {
        self.bump(); // opening `'`
        let start = self.pos;
        loop {
            match self.peek() {
                None | Some(b'\n') => return self.err("unterminated literal string"),
                Some(b'\'') => {
                    let s = self.src[start..self.pos].to_string();
                    self.pos += 1;
                    return Ok(s);
                }
                Some(_) => {
                    let ch = self.src[self.pos..].chars().next().unwrap();
                    self.pos += ch.len_utf8();
                }
            }
        }
    }

    fn parse_array(&mut self) -> Result<Value, Error> {
        self.bump(); // `[`
        let mut items = Vec::new();
        loop {
            self.skip_ws_and_comments()?;
            if self.peek() == Some(b']') {
                self.bump();
                return Ok(Value::Array(items));
            }
            items.push(self.parse_value()?);
            self.skip_ws_and_comments()?;
            match self.peek() {
                Some(b',') => {
                    self.bump();
                }
                Some(b']') => {
                    self.bump();
                    return Ok(Value::Array(items));
                }
                _ => return self.err("expected `,` or `]` in array"),
            }
        }
    }

    fn parse_inline_table(&mut self) -> Result<Value, Error> {
        self.bump(); // `{`
        let mut table = Table::new();
        self.skip_ws_inline();
        if self.peek() == Some(b'}') {
            self.bump();
            return Ok(Value::Table(table));
        }
        loop {
            self.skip_ws_inline();
            let path = self.parse_key_path()?;
            self.skip_ws_inline();
            if self.bump() != Some(b'=') {
                return self.err("expected `=` in inline table");
            }
            self.skip_ws_inline();
            let val = self.parse_value()?;
            let (parents, leaf) = path.split_at(path.len() - 1);
            let target = descend_mut(&mut table, parents, self.line)?;
            if target.contains_key(&leaf[0]) {
                return self.err(format!("duplicate key {:?}", leaf[0]));
            }
            target.insert(leaf[0].clone(), val);
            self.skip_ws_inline();
            match self.peek() {
                Some(b',') => {
                    self.bump();
                }
                Some(b'}') => {
                    self.bump();
                    return Ok(Value::Table(table));
                }
                _ => return self.err("expected `,` or `}` in inline table"),
            }
        }
    }

    fn parse_bool(&mut self) -> Result<Value, Error> {
        if self.src[self.pos..].starts_with("true")
            && !self
                .bytes
                .get(self.pos + 4)
                .is_some_and(|b| is_bare_key_byte(*b))
        {
            self.pos += 4;
            Ok(Value::Boolean(true))
        } else if self.src[self.pos..].starts_with("false")
            && !self
                .bytes
                .get(self.pos + 5)
                .is_some_and(|b| is_bare_key_byte(*b))
        {
            self.pos += 5;
            Ok(Value::Boolean(false))
        } else {
            self.err("invalid boolean literal")
        }
    }

    fn parse_integer(&mut self) -> Result<Value, Error> {
        let start = self.pos;
        if matches!(self.peek(), Some(b'+' | b'-')) {
            self.pos += 1;
        }
        while let Some(b) = self.peek() {
            if b.is_ascii_digit() || b == b'_' {
                self.pos += 1;
            } else {
                break;
            }
        }
        let cleaned: String = self.src[start..self.pos]
            .chars()
            .filter(|c| *c != '_')
            .collect();
        let n: i64 = cleaned.parse().map_err(|_| Error {
            line: self.line,
            message: format!("invalid integer {cleaned:?}"),
        })?;
        Ok(Value::Integer(n))
    }
}

fn is_bare_key_byte(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_' || b == b'-'
}

fn descend_mut<'a>(
    mut current: &'a mut Table,
    path: &[String],
    line: usize,
) -> Result<&'a mut Table, Error> {
    for segment in path {
        let entry = current
            .entry(segment.clone())
            .or_insert_with(|| Value::Table(Table::new()));
        current = match entry {
            Value::Table(t) => t,
            _ => {
                return Err(Error {
                    line,
                    message: format!("key {segment:?} is not a table"),
                });
            }
        };
    }
    Ok(current)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dotted_quoted_tables_and_multiline_arrays() {
        let doc = r#"
# comment
[maglev."15.2"]
phases = [
    "Maglev graph building",
    "Truncation",
]
"#;
        let t = parse_table(doc).unwrap();
        let phases = t["maglev"]
            .get("15.2")
            .unwrap()
            .get("phases")
            .unwrap()
            .as_array()
            .unwrap();
        assert_eq!(phases.len(), 2);
        assert_eq!(phases[0].as_str(), Some("Maglev graph building"));
    }

    #[test]
    fn rejects_syntax_errors() {
        assert!(parse_table("not toml [").is_err());
        assert!(parse_table("[keys]\na = 1\na = 2\n").is_err());
        assert!(parse_table("[keys]\na = \"unterminated\n").is_err());
    }
}
