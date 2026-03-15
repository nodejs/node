// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod parse_state;

use std::{borrow::Cow, fmt};

use nom::{
    branch::alt,
    bytes::complete::{tag, take, take_until, take_while1},
    character::complete::{hex_digit1, i32, multispace1, not_line_ending, u32},
    combinator::{eof, map, map_parser, map_res, opt, peek, value},
    error::{context, convert_error, make_error, ContextError, ParseError, VerboseError},
    multi::{many0, separated_list0},
    sequence::{delimited, pair, preceded, terminated},
    Finish, IResult, Parser,
};

use crate::bundle::{Int28, Key, Resource, ResourceBundle, Table};

use self::parse_state::ParseState;

/// Generates appropriate parsers for type ID strings.
///
/// A type may have one or more aliases. Note that if one type alias appears at
/// the beginning of another, the longer alias must appear first.
macro_rules! type_id {
    ($(#[$meta:meta])* $name:ident, $tag:literal) => {
        $(#[$meta])*
        fn $name<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
        where
            E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>
        {
            context(stringify!($name), preceded(tag(":"),
                tag($tag)
            ))(input)
        }
    };

    ($(#[$meta:meta])* $name:ident, $( $tag:literal ),+) => {
        $(#[$meta])*
        fn $name<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
        where
            E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>
        {
            context(stringify!($name), preceded(tag(":"), alt((
                $( tag($tag) ),+
            ))))(input)
        }
    };
}

type_id!(
    /// Reads a root table resource type ID. Note that `"(nofallback)"` may only
    /// appear on the root resource.
    root_table_type,
    "table(nofallback)",
    "table"
);
type_id!(
    /// Reads a string resource type ID.
    string_type,
    "string"
);
type_id!(
    /// Reads an array resource type ID.
    array_type,
    "array"
);
type_id!(
    /// Reads a table resource type ID.
    table_type,
    "table"
);
type_id!(
    /// Reads a binary resource type ID.
    binary_type,
    "binary",
    "bin"
);
type_id!(
    /// Reads an integer resource type ID.
    integer_type,
    "integer",
    "int"
);
type_id!(
    /// Reads an integer vector resource type ID.
    int_vector_type,
    "intvector"
);
type_id!(
    /// Reads an import resource type ID.
    import_type,
    "import"
);
type_id!(
    /// Reads an include resource type ID.
    include_type,
    "include"
);
type_id!(
    /// Reads a resource alias type ID.
    alias_type,
    "alias"
);

/// Reads any type ID which may appear as a leaf in the resource tree.
fn type_id_no_root<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "type_id",
        alt((
            string_type,
            array_type,
            table_type,
            binary_type,
            integer_type,
            int_vector_type,
            import_type,
            include_type,
            alias_type,
        )),
    )(input)
}

/// Reads one or more of the core set of ASCII characters used in keys.
///
/// It is presently unclear which characters appear in keys in practice, so in
/// the interest of a simplified parse, only characters which have been
/// encountered in real files are supported.
fn invariant_chars<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "invariant_chars",
        take_while1(|c: char| c.is_alphanumeric() || c == '-'),
    )(input)
}

/// Reads a single-line comment up to but not including the final newline.
fn eol_comment<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context("eol_comment", preceded(tag("//"), not_line_ending))(input)
}

/// Reads a multi-line comment.
///
/// Does not support nested comments.
fn delimited_comment<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "delimited_comment",
        delimited(tag("/*"), take_until("*/"), tag("*/")),
    )(input)
}

/// Reads one single-line or multi-line comment.
fn comment<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context("comment", alt((eol_comment, delimited_comment)))(input)
}

/// Reads a string.
///
/// Strings may be quote-delimited or not. While the partial specification
/// indicates that adjacent strings are combined, this is not supported in the
/// current version of the parser. No support for character escapes is
/// indicated.
///
/// The specification is unclear on how string encoding is to be handled, so
/// this parser assumes that strings are to be well-formed in the same encoding
/// as the rest of the file and representable in Unicode.
///
/// The specification also allows for the use of `\u` and `\U` literals. Support
/// for these has also been omitted for the time being.
///
/// See [`Reader`] for more details on the specification.
fn string<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, &'a str, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "string",
        map(
            token(alt((
                delimited(tag("\""), take_until("\""), tag("\"")),
                invariant_chars,
            ))),
            |value| value.input(),
        ),
    )(input)
}

/// Reads a signed or unsigned 32-bit integer.
fn integer<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, u32, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context("integer", token(alt((u32, map(i32, |value| value as u32)))))(input)
}

/// Reads and discards any number of comments and any amount of whitespace.
fn discardable<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, (), E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context("discardable", value((), many0(alt((comment, multispace1)))))(input)
}

/// Reads a single token as specified by the provided parser, surrounded by any
/// number of comments and any amount of whitespace.
fn token<'a, F, O, E>(mut parser: F) -> impl FnMut(ParseState<'a>) -> IResult<ParseState<'a>, O, E>
where
    F: Parser<ParseState<'a>, O, E>,
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context("token", move |input: ParseState<'a>| {
        let (rest, _) = discardable.parse(input)?;
        let (rest, token) = parser.parse(rest)?;
        let (rest, _) = discardable.parse(rest)?;

        Ok((rest, token))
    })
}

/// Generates appropriate matchers for simple string tokens.
macro_rules! simple_token {
    ($name:ident, $str:expr) => {
        fn $name<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ParseState<'a>, E>
        where
            E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
        {
            context(stringify!($name), token(tag($str)))(input)
        }
    };
}

simple_token!(left_brace, "{");
simple_token!(right_brace, "}");
simple_token!(comma, ",");

/// Reads a table key.
fn key<'a, E>(state: ParseState<'a>) -> IResult<ParseState<'a>, Key<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    let input = state.clone();
    context(
        "key",
        map(
            terminated(
                string,
                // If the string is not followed by a resource, as indicated by
                // the presence of a type ID or resource delimiter, it is not a
                // key and we shouldn't record it.
                peek(alt((type_id_no_root, left_brace))),
            ),
            |key| {
                let key = Key::from(key);
                state.encounter_key(key.clone());

                key
            },
        ),
    )(input)
}

/// Reads a single table entry, consisting of a key and an associated resource.
fn table_entry<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, (Key<'a>, Resource<'a>), E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context("table_entry", pair(key, resource))(input)
}

/// Generates a parser for a resource body, consisting of a resource
/// type-specific body layout delimited by curly braces.
macro_rules! resource_body {
    ($body_parser:expr) => {
        delimited(left_brace, $body_parser, right_brace)
    };
}

/// Generates a parser for a resource with a mandatory type ID.
macro_rules! resource {
    ($type:ident, $body_parser:expr) => {
        preceded($type, resource_body!($body_parser))
    };
}

/// Generates a parser for a resource with an optional type ID.
macro_rules! resource_opt_tag {
    ($type:ident, $body_parser:expr) => {
        preceded(opt($type), resource_body!($body_parser))
    };
}

/// Reads the body of a table resource, consisting of any number of key-resource
/// pair entries.
fn table_body<'a, E>(state: ParseState<'a>) -> IResult<ParseState<'a>, Table<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    let (state, entries) = context("table_body", many0(table_entry)).parse(state)?;

    // Build the table struct itself.
    let mut table = Table::new();
    for (k, v) in entries {
        table.insert(k, v);
    }

    Ok((state, table))
}

/// Reads a table resource, consisting of key-resource pairs.
///
/// Though it is not required in the resource bundle text, the final in-memory
/// representation sorts these pairs lexically by key.
fn table_resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "table_resource",
        map(resource_opt_tag!(table_type, table_body), Resource::Table),
    )(input)
}

/// Reads the root table resource, noting whether locale fallback is disabled.
fn root_table_resource<'a, E>(
    input: ParseState<'a>,
) -> IResult<ParseState<'a>, (bool, Resource<'a>), E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    // Determine whether locale fallback is enabled by the type ID of the table,
    // if present. The type ID is optional for tables, so it's okay if we don't
    // find one, at which point we default to enabling locale fallback.
    let (input, is_fallback_enabled) = opt(root_table_type).parse(input)?;
    let is_fallback_enabled =
        is_fallback_enabled.is_none_or(|type_id| type_id.input() != "table(nofallback)");

    // Read the body of the root resource itself.
    let (input, resource) = map(resource_body!(table_body), Resource::Table).parse(input)?;

    Ok((input, (is_fallback_enabled, resource)))
}

/// Reads an array resource consisting of any number of hetereogeneously-typed
/// resources, optionally separated by commas.
fn array_resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    // Note that `nom`'s `separated_list0` parser requires that the separator
    // always consume, so we can't use it for optional commas. However, the
    // partial specification allows for trailing commas, so `terminated` works
    // fine here.
    context(
        "array_resource",
        map(
            resource_opt_tag!(array_type, many0(terminated(resource, opt(comma)))),
            Resource::Array,
        ),
    )(input)
}

/// Reads an integer resource, consisting of a single 28-bit integer.
///
/// These integers have no inherent signedness; consumers specify whether a
/// signed or unsigned integer is desired. Because of this, note that 28-bit
/// integers require special handling in-memory. See [`Int28`] for more details.
fn int_resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "int_resource",
        map(resource!(integer_type, integer), |value| {
            Resource::Integer(Int28::from(value))
        }),
    )(input)
}

/// Reads an integer vector resource, consisting of any number of 32-bit
/// integers separated by commas. A trailing comma is optional.
fn int_vector_resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "int_vector_resource",
        map(
            resource!(
                int_vector_type,
                terminated(separated_list0(comma, integer), opt(comma))
            ),
            Resource::IntVector,
        ),
    )(input)
}

/// Reads a single byte represented as a pair of hex digits, each representing a
/// nibble of the byte. The high nibble is listed first.
fn binary_byte(input: &str) -> IResult<&str, u8> {
    context(
        "binary_byte",
        map_res(map_parser(take(2usize), hex_digit1), |word| {
            u8::from_str_radix(word, 16)
        }),
    )(input)
}

/// Reads a binary resource, consisting of a string of any number of paired hex
/// digits representing byte values.
///
/// See [`binary_byte`].
fn binary_resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    let (rest, string) = resource!(binary_type, string).parse(input.clone())?;

    let (_, elements) = match many0(binary_byte).parse(string) {
        Ok(elements) => elements,
        Err(err) => {
            println!("{err}");
            return Err(nom::Err::Error(make_error(
                input,
                nom::error::ErrorKind::HexDigit,
            )));
        }
    };

    Ok((rest, Resource::Binary(Cow::from(elements))))
}

/// Reads a string resource.
///
/// Unlike other resources, a string may appear bare, without delimiting
/// braces.
///
/// See [`string`] for more details on the representation of strings.
fn string_resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "string_resource",
        map(
            alt((resource_opt_tag!(string_type, string), string)),
            |value| Resource::String(Cow::from(value)),
        ),
    )(input)
}

/// Reads a single resource.
fn resource<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, Resource<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "resource",
        alt((
            int_resource,
            int_vector_resource,
            string_resource,
            binary_resource,
            table_resource,
            array_resource,
        )),
    )(input)
}

/// Reads a complete resource bundle.
fn bundle<'a, E>(input: ParseState<'a>) -> IResult<ParseState<'a>, ResourceBundle<'a>, E>
where
    E: ParseError<ParseState<'a>> + ContextError<ParseState<'a>>,
{
    context(
        "bundle",
        map(
            terminated(
                pair(
                    // Note that the name of the bundle is a string and not a key,
                    // and it is not included in the list of keys in the bundle.
                    string,
                    root_table_resource,
                ),
                eof,
            ),
            |(name, (is_locale_fallback_enabled, root))| {
                ResourceBundle::new(Cow::from(name), root, is_locale_fallback_enabled)
            },
        ),
    )(input)
}

/// The `Reader` struct provides a means of parsing a [`ResourceBundle`] from
/// a string representing the contents of a text-based resource bundle file.
///
/// Note that the partial [specification] of the text-based format allows for
/// files to be in encodings other than UTF-8 and indicates that autodetection
/// of encoding should occur. At present, `Reader` only supports input as
/// UTF-8.
///
/// [specification]: https://github.com/unicode-org/icu-docs/blob/main/design/bnf_rb.txt
#[derive(Debug)]
#[non_exhaustive]
pub struct Reader;

impl Reader {
    /// Parses the given string into a resource bundle.
    ///
    /// Returns the parsed resource bundle and a list of the keys encountered in
    /// the resource bundle in the order they were encountered.
    pub fn read(input: &str) -> Result<(ResourceBundle<'_>, Vec<Key<'_>>), TextParserError> {
        let input = ParseState::new(input);

        let (final_state, bundle) = bundle::<VerboseError<ParseState>>(input.clone())
            .finish()
            .map_err(|err| TextParserError {
                trace: convert_error(input, err),
            })?;

        let keys_in_discovery_order = final_state.take_keys().into_iter().collect();

        Ok((bundle, keys_in_discovery_order))
    }
}

#[derive(Debug)]
pub struct TextParserError {
    trace: String,
}

impl fmt::Display for TextParserError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_fmt(format_args!(
            "Parse error while reading text bundle:\n{}",
            self.trace
        ))
    }
}
