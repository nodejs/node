// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::ast;
use super::lexer::{Lexer, Token};
use alloc::string::String;
use alloc::string::ToString;
use alloc::vec;
use alloc::vec::Vec;
use core::iter::Peekable;
use displaydoc::Display;

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
#[derive(Display, Debug, PartialEq, Eq, Copy, Clone)]
#[non_exhaustive]
pub enum ParseError {
    #[displaydoc("expected 'AND' condition")]
    ExpectedAndCondition,
    #[displaydoc("expected relation")]
    ExpectedRelation,
    #[displaydoc("expected operator")]
    ExpectedOperator,
    #[displaydoc("expected operand")]
    ExpectedOperand,
    #[displaydoc("expected value")]
    ExpectedValue,
    #[displaydoc("expected sample type")]
    ExpectedSampleType,
    #[displaydoc("Value too large")]
    ValueTooLarge,
}

impl core::error::Error for ParseError {}

/// Unicode Plural Rule parser converts an
/// input string into a Rule [`AST`].
///
/// A single [`Rule`] contains a [`Condition`] and optionally a set of
/// [`Samples`].
///
/// A [`Condition`] can be then used by the [`test_condition`] to test
/// against [`PluralOperands`], to find the appropriate [`PluralCategory`].
///
/// [`Samples`] are useful for tooling to help translators understand examples of numbers
/// covered by the given [`Rule`].
///
/// At runtime, only the [`Condition`] is used and for that, consider using [`parse_condition`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```
/// use icu::plurals::provider::rules::reference::parse;
///
/// let input = b"i = 0 or n = 1 @integer 0, 1 @decimal 0.0~1.0, 0.00~0.04";
/// assert!(parse(input).is_ok());
/// ```
///
/// [`AST`]: super::ast
/// [`test_condition`]: super::test_condition
/// [`PluralOperands`]: crate::PluralOperands
/// [`PluralCategory`]: crate::PluralCategory
/// [`Rule`]: super::ast::Rule
/// [`Samples`]: super::ast::Samples
/// [`Condition`]:  super::ast::Condition
/// [`parse_condition`]: parse_condition()
pub fn parse(input: &[u8]) -> Result<ast::Rule, ParseError> {
    let parser = Parser::new(input);
    parser.parse()
}

/// Unicode Plural Rule parser converts an
/// input string into an [`AST`].
///
/// That [`AST`] can be then used by the [`test_condition`] to test
/// against [`PluralOperands`], to find the appropriate [`PluralCategory`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```
/// use icu::plurals::provider::rules::reference::parse_condition;
///
/// let input = b"i = 0 or n = 1";
/// assert!(parse_condition(input).is_ok());
/// ```
///
/// [`AST`]: super::ast
/// [`test_condition`]: super::test_condition
/// [`PluralOperands`]: crate::PluralOperands
/// [`PluralCategory`]: crate::PluralCategory
#[cfg(feature = "datagen")]
pub fn parse_condition(input: &[u8]) -> Result<ast::Condition, ParseError> {
    let parser = Parser::new(input);
    parser.parse_condition()
}

struct Parser<'p> {
    lexer: Peekable<Lexer<'p>>,
}

impl<'p> Parser<'p> {
    fn new(input: &'p [u8]) -> Self {
        Self {
            lexer: Lexer::new(input).peekable(),
        }
    }

    pub fn parse(mut self) -> Result<ast::Rule, ParseError> {
        self.get_rule()
    }

    #[cfg(feature = "datagen")]
    pub fn parse_condition(mut self) -> Result<ast::Condition, ParseError> {
        self.get_condition()
    }

    fn get_rule(&mut self) -> Result<ast::Rule, ParseError> {
        Ok(ast::Rule {
            condition: self.get_condition()?,
            samples: self.get_samples()?,
        })
    }

    fn get_condition(&mut self) -> Result<ast::Condition, ParseError> {
        let mut result = vec![];

        if let Some(cond) = self.get_and_condition()? {
            result.push(cond);
        } else {
            return Ok(ast::Condition(result));
        }

        while self.take_if(Token::Or) {
            if let Some(cond) = self.get_and_condition()? {
                result.push(cond);
            } else {
                return Err(ParseError::ExpectedAndCondition);
            }
        }
        // If lexer is not done, error?
        Ok(ast::Condition(result))
    }

    fn get_and_condition(&mut self) -> Result<Option<ast::AndCondition>, ParseError> {
        if let Some(relation) = self.get_relation()? {
            let mut rel = vec![relation];

            while self.take_if(Token::And) {
                if let Some(relation) = self.get_relation()? {
                    rel.push(relation);
                } else {
                    return Err(ParseError::ExpectedRelation);
                }
            }
            Ok(Some(ast::AndCondition(rel)))
        } else {
            Ok(None)
        }
    }

    fn get_relation(&mut self) -> Result<Option<ast::Relation>, ParseError> {
        if let Some(expression) = self.get_expression()? {
            let operator = match self.lexer.next() {
                Some(Token::Operator(op)) => op,
                _ => return Err(ParseError::ExpectedOperator),
            };
            let range_list = self.get_range_list()?;
            Ok(Some(ast::Relation {
                expression,
                operator,
                range_list,
            }))
        } else {
            Ok(None)
        }
    }

    fn get_expression(&mut self) -> Result<Option<ast::Expression>, ParseError> {
        let operand = match self.lexer.peek() {
            Some(Token::E) => ast::Operand::E,
            Some(Token::Operand(op)) => *op,
            Some(Token::At) | None => return Ok(None),
            _ => return Err(ParseError::ExpectedOperand),
        };
        self.lexer.next();
        let modulus = if self.take_if(Token::Modulo) {
            Some(self.get_value()?)
        } else {
            None
        };
        Ok(Some(ast::Expression { operand, modulus }))
    }

    fn get_range_list(&mut self) -> Result<ast::RangeList, ParseError> {
        let mut range_list = Vec::with_capacity(1);
        loop {
            range_list.push(self.get_range_list_item()?);
            if !self.take_if(Token::Comma) {
                break;
            }
        }
        Ok(ast::RangeList(range_list))
    }

    fn take_if(&mut self, token: Token) -> bool {
        if self.lexer.peek() == Some(&token) {
            self.lexer.next();
            true
        } else {
            false
        }
    }

    fn get_range_list_item(&mut self) -> Result<ast::RangeListItem, ParseError> {
        let value = self.get_value()?;
        if self.take_if(Token::DotDot) {
            let value2 = self.get_value()?;
            Ok(ast::RangeListItem::Range(value..=value2))
        } else {
            Ok(ast::RangeListItem::Value(value))
        }
    }

    fn get_value(&mut self) -> Result<ast::Value, ParseError> {
        match self.lexer.next() {
            Some(Token::Number(v)) => Ok(ast::Value(v as u64)),
            Some(Token::Zero) => Ok(ast::Value(0)),
            _ => Err(ParseError::ExpectedValue),
        }
    }

    fn get_samples(&mut self) -> Result<Option<ast::Samples>, ParseError> {
        let mut integer = None;
        let mut decimal = None;

        while self.take_if(Token::At) {
            match self.lexer.next() {
                Some(Token::Integer) => integer = Some(self.get_sample_list()?),
                Some(Token::Decimal) => decimal = Some(self.get_sample_list()?),
                _ => return Err(ParseError::ExpectedSampleType),
            };
        }
        if integer.is_some() || decimal.is_some() {
            Ok(Some(ast::Samples { integer, decimal }))
        } else {
            Ok(None)
        }
    }

    fn get_sample_list(&mut self) -> Result<ast::SampleList, ParseError> {
        let mut ranges = vec![self.get_sample_range()?];
        let mut ellipsis = false;

        while self.take_if(Token::Comma) {
            if self.take_if(Token::Ellipsis) {
                ellipsis = true;
                break;
            }
            ranges.push(self.get_sample_range()?);
        }
        Ok(ast::SampleList {
            sample_ranges: ranges,
            ellipsis,
        })
    }

    fn get_sample_range(&mut self) -> Result<ast::SampleRange, ParseError> {
        let lower_val = self.get_decimal_value()?;
        let upper_val = if self.take_if(Token::Tilde) {
            Some(self.get_decimal_value()?)
        } else {
            None
        };
        Ok(ast::SampleRange {
            lower_val,
            upper_val,
        })
    }

    fn get_decimal_value(&mut self) -> Result<ast::DecimalValue, ParseError> {
        let mut s = String::new();
        loop {
            match self.lexer.peek() {
                Some(Token::Zero) => s.push('0'),
                Some(Token::Number(v)) => {
                    s.push_str(&v.to_string());
                }
                _ => {
                    break;
                }
            }
            self.lexer.next();
        }
        if self.take_if(Token::Dot) {
            s.push('.');
            loop {
                match self.lexer.peek() {
                    Some(Token::Zero) => s.push('0'),
                    Some(Token::Number(v)) => {
                        s.push_str(&v.to_string());
                    }
                    _ => {
                        break;
                    }
                }
                self.lexer.next();
            }
        }

        if self.take_if(Token::E) {
            s.push('e');
            match self.lexer.peek() {
                Some(Token::Zero) => s.push('0'),
                Some(Token::Number(v)) => {
                    s.push_str(&v.to_string());
                }
                _ => {
                    return Err(ParseError::ExpectedValue);
                }
            }
            self.lexer.next();
        }
        if s.is_empty() {
            Err(ParseError::ExpectedValue)
        } else {
            Ok(ast::DecimalValue(s))
        }
    }
}
