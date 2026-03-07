#![allow(dead_code)]

use std::{borrow::Cow, fmt::Debug};

use swc_atoms::Atom;
use swc_common::{
    errors::{DiagnosticBuilder, Handler},
    Span, Spanned,
};

/// Note: this struct is 8 bytes.
#[derive(Debug, Clone, PartialEq)]
pub struct Error {
    error: Box<(Span, SyntaxError)>,
}

impl Spanned for Error {
    fn span(&self) -> Span {
        (*self.error).0
    }
}

impl Error {
    #[cold]
    pub fn new(span: Span, error: SyntaxError) -> Self {
        Self {
            error: Box::new((span, error)),
        }
    }

    pub fn kind(&self) -> &SyntaxError {
        &self.error.1
    }

    pub fn into_kind(self) -> SyntaxError {
        self.error.1
    }
}

#[derive(Debug, Clone, PartialEq)]
#[non_exhaustive]
pub enum SyntaxError {
    Eof,
    DeclNotAllowed,

    UsingDeclNotAllowed,
    UsingDeclNotAllowedForForInLoop,
    UsingDeclNotEnabled,
    InvalidNameInUsingDecl,
    InitRequiredForUsingDecl,

    PrivateNameInInterface,

    InvalidSuperCall,
    InvalidSuper,
    InvalidSuperPrivateName,

    InvalidNewTarget,

    InvalidImport,

    ArrowNotAllowed,
    ExportNotAllowed,
    GetterSetterCannotBeReadonly,
    GetterSetterCannotBeOptional,
    GetterParam,
    SetterParam,

    TopLevelAwaitInScript,

    LegacyDecimal,
    LegacyOctal,
    InvalidIdentChar,
    ExpectedDigit {
        radix: u8,
    },
    SetterParamRequired,
    RestPatInSetter,

    UnterminatedBlockComment,
    UnterminatedStrLit,
    ExpectedUnicodeEscape,
    EscapeInReservedWord {
        word: Atom,
    },
    UnterminatedRegExp,
    UnterminatedTpl,
    IdentAfterNum,
    UnexpectedChar {
        c: char,
    },
    InvalidStrEscape,
    InvalidUnicodeEscape,
    BadCharacterEscapeSequence {
        expected: &'static str,
    },
    NumLitTerminatedWithExp,
    LegacyCommentInModule,

    /// "implements", "interface", "let", "package", "private", "protected",
    /// "public", "static", or "yield"
    InvalidIdentInStrict(Atom),

    InvalidIdentInAsync,
    /// 'eval' and 'arguments' are invalid identifier in strict mode.
    EvalAndArgumentsInStrict,
    ArgumentsInClassField,
    IllegalLanguageModeDirective,
    UnaryInExp {
        left: String,
        left_span: Span,
    },
    Hash,
    LineBreakInThrow,
    LineBreakBeforeArrow,

    /// Unexpected token
    Unexpected {
        got: String,
        expected: &'static str,
    },
    UnexpectedTokenWithSuggestions {
        candidate_list: Vec<&'static str>,
    },
    ReservedWordInImport,
    AssignProperty,
    Expected(String, String),
    ExpectedSemiForExprStmt {
        expr: Span,
    },

    AwaitStar,
    ReservedWordInObjShorthandOrPat,

    NullishCoalescingWithLogicalOp,

    MultipleDefault {
        /// Span of the previous default case
        previous: Span,
    },
    CommaAfterRestElement,
    NonLastRestParam,
    SpreadInParenExpr,
    /// `()`
    EmptyParenExpr,
    InvalidPat,
    InvalidExpr,
    NotSimpleAssign,
    InvalidAssignTarget,
    ExpectedIdent,
    ExpectedSemi,
    DuplicateLabel(Atom),
    AsyncGenerator,
    NonTopLevelImportExport,
    ImportExportInScript,
    ImportMetaInScript,
    PatVarWithoutInit,
    WithInStrict,
    ReturnNotAllowed,
    TooManyVarInForInHead,
    VarInitializerInForInHead,
    LabelledGeneratorOrAsync,
    LabelledFunctionInStrict,
    YieldParamInGen,
    AwaitParamInAsync,

    AwaitForStmt,

    AwaitInFunction,

    UnterminatedJSXContents,
    EmptyJSXAttr,
    InvalidJSXValue,
    JSXExpectedClosingTagForLtGt,
    JSXExpectedClosingTag {
        tag: Atom,
    },
    InvalidLeadingDecorator,
    DecoratorOnExport,

    TsRequiredAfterOptional,
    TsInvalidParamPropPat,

    SpaceBetweenHashAndIdent,

    AsyncConstructor,
    PropertyNamedConstructor,
    PrivateConstructor,
    PrivateNameModifier(Atom),
    ConstructorAccessor,
    ReadOnlyMethod,
    GeneratorConstructor,
    DuplicateConstructor,
    TsBindingPatCannotBeOptional,
    SuperCallOptional,
    OptChainCannotFollowConstructorCall,
    TaggedTplInOptChain,

    TrailingCommaInsideImport,
    ImportRequiresOneOrTwoArgs,

    ExportDefaultWithOutFrom,
    ExportExpectFrom(Atom),

    DotsWithoutIdentifier,

    NumericSeparatorIsAllowedOnlyBetweenTwoDigits,

    ImportBindingIsString(Atom),
    ExportBindingIsString,

    ConstDeclarationsRequireInitialization,

    DuplicatedRegExpFlags(char),
    UnknownRegExpFlags,

    TS1003,
    TS1005,
    TS1009,
    TS1014,
    TS1015,
    TS1029(Atom, Atom),
    TS1030(Atom),
    TS1031,
    TS1038,
    TS1042,
    TS1047,
    TS1048,
    TS1056,
    TS1085,
    TS1089(Atom),
    TS1092,
    TS1096,
    TS1098,
    TS1100,
    TS1102,
    TS1105,
    TS1106,
    TS1107,
    TS1109,
    TS1110,
    TS1114,
    TS1115,
    TS1116,
    TS1123,
    TS1141,
    TS1162,
    TS1164,
    TS1171,
    TS1172,
    TS1173,
    TS1174,
    TS1175,
    TS1183,
    TS1184,
    TS1185,
    TS1093,
    TS1196,
    TS1242,
    TS1243(Atom, Atom),
    TS1244,
    TS1245,
    TS1267,
    TS1273(Atom),
    TS1274(Atom),
    TS1277(Atom),
    TS2206,
    TS2207,
    TS2369,
    TS2371,
    TS2406,
    TS2410,
    TS2414,
    TS2427,
    TS2452,
    TS2483,
    TS2491,
    TS2499,
    TS2703,
    TS4112,
    TS8038,
    TS18010,
    TSTypeAnnotationAfterAssign,
    TsNonNullAssertionNotAllowed(Atom),

    WithLabel {
        inner: Box<Error>,
        span: Span,
        note: &'static str,
    },

    ReservedTypeAssertion,
    ReservedArrowTypeParam,
    EmptyTypeArgumentList,
}

impl SyntaxError {
    #[cold]
    #[inline(never)]
    pub fn msg(&self) -> Cow<'static, str> {
        match self {
            SyntaxError::PrivateNameInInterface => {
                "private names are not allowed in interface".into()
            }
            SyntaxError::TopLevelAwaitInScript => {
                "top level await is only allowed in module".into()
            }
            SyntaxError::LegacyDecimal => {
                "Legacy decimal escape is not permitted in strict mode".into()
            }
            SyntaxError::LegacyOctal => {
                "Legacy octal escape is not permitted in strict mode".into()
            }
            SyntaxError::InvalidIdentChar => "Invalid character in identifier".into(),
            SyntaxError::ExpectedDigit { radix } => format!(
                "Expected {} digit",
                match radix {
                    2 => "a binary",
                    8 => "an octal",
                    10 => "a decimal",
                    16 => "a hexadecimal",
                    _ => unreachable!(),
                }
            )
            .into(),
            SyntaxError::UnterminatedBlockComment => "Unterminated block comment".into(),
            SyntaxError::UnterminatedStrLit => "Unterminated string constant".into(),
            SyntaxError::ExpectedUnicodeEscape => "Expected unicode escape".into(),
            SyntaxError::EscapeInReservedWord { ref word } => {
                format!("Unexpected escape sequence in reserved word: {word}").into()
            }
            SyntaxError::UnterminatedRegExp => "Unterminated regexp literal".into(),
            SyntaxError::UnterminatedTpl => "Unterminated template".into(),
            SyntaxError::IdentAfterNum => "Identifier cannot follow number".into(),
            SyntaxError::UnexpectedChar { c } => format!("Unexpected character {c:?}").into(),
            SyntaxError::InvalidStrEscape => "Invalid string escape".into(),
            SyntaxError::InvalidUnicodeEscape => "Invalid unicode escape".into(),
            SyntaxError::BadCharacterEscapeSequence { expected } => {
                format!("Bad character escape sequence, expected {expected}").into()
            }
            SyntaxError::LegacyCommentInModule => {
                "Legacy comments cannot be used in module code".into()
            }
            SyntaxError::NumLitTerminatedWithExp => "Expected +, - or decimal digit after e".into(),

            SyntaxError::InvalidIdentInStrict(identifier_name) => {
                format!("`{identifier_name}` cannot be used as an identifier in strict mode").into()
            }
            SyntaxError::InvalidIdentInAsync => {
                "`await` cannot be used as an identifier in an async context".into()
            }
            SyntaxError::EvalAndArgumentsInStrict => "'eval' and 'arguments' cannot be used as a \
                                                      binding identifier in strict mode"
                .into(),
            SyntaxError::ArgumentsInClassField => {
                "'arguments' is only allowed in functions and class methods".into()
            }
            SyntaxError::IllegalLanguageModeDirective => {
                "Illegal 'use strict' directive in function with non-simple parameter list.".into()
            }
            SyntaxError::UnaryInExp { .. } => {
                "'**' cannot be applied to unary/await expression.".into()
            }
            SyntaxError::Hash => "Unexpected token '#'".into(),
            SyntaxError::LineBreakInThrow => "LineBreak cannot follow 'throw'".into(),
            SyntaxError::LineBreakBeforeArrow => {
                "Unexpected line break between arrow head and arrow".into()
            }
            SyntaxError::Unexpected {
                ref got,
                ref expected,
            } => format!("Unexpected token `{got}`. Expected {expected}").into(),

            SyntaxError::ReservedWordInImport => "cannot import as reserved word".into(),
            SyntaxError::AssignProperty => "assignment property is invalid syntax".into(),
            SyntaxError::Expected(token, ref got) => {
                format!("Expected '{token}', got '{got}'").into()
            }
            SyntaxError::ExpectedSemiForExprStmt { .. } => "Expected ';', '}' or <eof>".into(),

            SyntaxError::AwaitStar => "await* has been removed from the async functions proposal. \
                                       Use Promise.all() instead."
                .into(),

            SyntaxError::ReservedWordInObjShorthandOrPat => {
                "Cannot use a reserved word as a shorthand property".into()
            }

            SyntaxError::MultipleDefault { .. } => {
                "A switch block cannot have multiple defaults".into()
            }
            SyntaxError::CommaAfterRestElement => {
                "Trailing comma isn't permitted after a rest element".into()
            }
            SyntaxError::NonLastRestParam => "Rest element must be final element".into(),
            SyntaxError::SpreadInParenExpr => {
                "Parenthesized expression cannot contain spread operator".into()
            }
            SyntaxError::EmptyParenExpr => "Parenthesized expression cannot be empty".into(),
            SyntaxError::InvalidPat => "Not a pattern".into(),
            SyntaxError::InvalidExpr => "Not an expression".into(),
            // TODO
            SyntaxError::NotSimpleAssign => "Cannot assign to this".into(),
            SyntaxError::ExpectedIdent => "Expected ident".into(),
            SyntaxError::ExpectedSemi => "Expected ';' or line break".into(),
            SyntaxError::DuplicateLabel(ref label) => {
                format!("Label {label} is already declared").into()
            }
            SyntaxError::AsyncGenerator => "An async function cannot be generator".into(),
            SyntaxError::NonTopLevelImportExport => {
                "'import', and 'export' are not permitted here".into()
            }
            SyntaxError::ImportExportInScript => {
                "'import', and 'export' cannot be used outside of module code".into()
            }
            SyntaxError::ImportMetaInScript => {
                "'import.meta' cannot be used outside of module code.".into()
            }

            SyntaxError::PatVarWithoutInit => "Destructuring bindings require initializers".into(),
            SyntaxError::WithInStrict => "With statement are not allowed in strict mode".into(),
            SyntaxError::ReturnNotAllowed => "Return statement is not allowed here".into(),
            SyntaxError::TooManyVarInForInHead => "Expected one variable binding".into(),
            SyntaxError::VarInitializerInForInHead => {
                "Unexpected initializer in for in/of loop".into()
            }
            SyntaxError::LabelledGeneratorOrAsync => {
                "Generator or async function cannot be labelled".into()
            }
            SyntaxError::LabelledFunctionInStrict => {
                "Function cannot be labelled in strict mode".into()
            }
            SyntaxError::YieldParamInGen => {
                "'yield' cannot be used as a parameter within generator".into()
            }
            SyntaxError::AwaitParamInAsync => {
                "`await` expressions cannot be used in a parameter initializer.".into()
            }
            SyntaxError::AwaitForStmt => {
                "for await syntax is valid only for for-of statement".into()
            }

            SyntaxError::AwaitInFunction => "await isn't allowed in non-async function".into(),

            SyntaxError::UnterminatedJSXContents => "Unterminated JSX contents".into(),
            SyntaxError::EmptyJSXAttr => {
                "JSX attributes must only be assigned a non-empty expression".into()
            }
            SyntaxError::InvalidJSXValue => {
                "JSX value should be either an expression or a quoted JSX text".into()
            }
            SyntaxError::JSXExpectedClosingTagForLtGt => {
                "Expected corresponding JSX closing tag for <>".into()
            }
            SyntaxError::JSXExpectedClosingTag { ref tag } => {
                format!("Expected corresponding JSX closing tag for <{tag}>").into()
            }
            SyntaxError::InvalidLeadingDecorator => {
                "Leading decorators must be attached to a class declaration".into()
            }
            SyntaxError::DecoratorOnExport => "Using the export keyword between a decorator and a \
                                               class is not allowed. Please use `export @dec \
                                               class` instead."
                .into(),
            SyntaxError::TsRequiredAfterOptional => {
                "A required element cannot follow an optional element.".into()
            }
            SyntaxError::SuperCallOptional => "Super call cannot be optional".into(),
            SyntaxError::OptChainCannotFollowConstructorCall => {
                "Constructor in/after an optional chaining is not allowed.".into()
            }
            SyntaxError::TaggedTplInOptChain => {
                "Tagged template literal is not allowed in optional chain.".into()
            }
            SyntaxError::TsInvalidParamPropPat => {
                "Typescript parameter property must be an identifier or assignment pattern".into()
            }
            SyntaxError::SpaceBetweenHashAndIdent => {
                "Unexpected space between # and identifier".into()
            }
            SyntaxError::AsyncConstructor => "Constructor can't be an async function".into(),
            SyntaxError::PropertyNamedConstructor => {
                "Classes may not have a non-static field named 'constructor'".into()
            }
            SyntaxError::PrivateConstructor => {
                "Classes can't have a private field named '#constructor'.".into()
            }
            SyntaxError::DuplicateConstructor => "A class can only have one constructor".into(),
            SyntaxError::PrivateNameModifier(modifier) => {
                format!("'{modifier}' modifier cannot be used with a private identifier").into()
            }
            SyntaxError::ConstructorAccessor => "Class constructor can't be an accessor.".into(),

            SyntaxError::ReadOnlyMethod => "A method cannot be readonly".into(),
            SyntaxError::TsBindingPatCannotBeOptional => "A binding pattern parameter cannot be \
                                                          optional in an implementation signature."
                .into(),

            SyntaxError::TrailingCommaInsideImport => {
                "Trailing comma is disallowed inside import(...) arguments".into()
            }

            SyntaxError::ImportRequiresOneOrTwoArgs => {
                "`import()` requires exactly one or two arguments".into()
            }

            SyntaxError::ExportDefaultWithOutFrom => {
                "export default statements required from '...';".into()
            }

            SyntaxError::ExportExpectFrom(s) => {
                format!("`{s}` cannot be used without `from` clause").into()
            }

            SyntaxError::DotsWithoutIdentifier => {
                "`...` must be followed by an identifier in declaration contexts".into()
            }

            SyntaxError::NumericSeparatorIsAllowedOnlyBetweenTwoDigits => {
                "A numeric separator is only allowed between two digits".into()
            }

            SyntaxError::NullishCoalescingWithLogicalOp => {
                "Nullish coalescing operator(??) requires parens when mixing with logical operators"
                    .into()
            }

            SyntaxError::TS1056 => {
                "jsc.target should be es5 or upper to use getter / setter".into()
            }
            SyntaxError::TS1110 => "type expected".into(),
            SyntaxError::TS1141 => "literal in an import type should be string literal".into(),

            SyntaxError::Eof => "Unexpected eof".into(),

            SyntaxError::TS2703 => {
                "The operand of a delete operator must be a property reference.".into()
            }
            SyntaxError::DeclNotAllowed => "Declaration is not allowed".into(),
            SyntaxError::UsingDeclNotAllowed => "Using declaration is not allowed".into(),
            SyntaxError::UsingDeclNotAllowedForForInLoop => {
                "Using declaration is not allowed in for-in loop".into()
            }
            SyntaxError::UsingDeclNotEnabled => "Using declaration is not enabled. Set \
                                                 jsc.parser.explicitResourceManagement to true"
                .into(),
            SyntaxError::InvalidNameInUsingDecl => {
                "Using declaration only allows identifiers".into()
            }
            SyntaxError::InitRequiredForUsingDecl => {
                "Using declaration requires initializer".into()
            }
            SyntaxError::InvalidSuperCall => "Invalid `super()`".into(),
            SyntaxError::InvalidSuper => "Invalid access to super".into(),
            SyntaxError::InvalidSuperPrivateName => {
                "Index super with private name is not allowed".into()
            }
            SyntaxError::InvalidNewTarget => "'new.target' is only allowed in the body of a \
                                              function declaration, function expression, or class."
                .into(),
            SyntaxError::InvalidImport => "Import is not allowed here".into(),
            SyntaxError::ArrowNotAllowed => "An arrow function is not allowed here".into(),
            SyntaxError::ExportNotAllowed => "`export` is not allowed here".into(),
            SyntaxError::GetterSetterCannotBeReadonly => {
                "A getter or a setter cannot be readonly".into()
            }
            SyntaxError::GetterSetterCannotBeOptional => {
                "A getter or a setter cannot be optional".into()
            }
            SyntaxError::GetterParam => "A `get` accessor cannot have parameters".into(),
            SyntaxError::SetterParam => "A `set` accessor must have exactly one parameter".into(),
            SyntaxError::RestPatInSetter => "Rest pattern is not allowed in setter".into(),

            SyntaxError::GeneratorConstructor => "A constructor cannot be generator".into(),

            SyntaxError::ImportBindingIsString(s) => format!(
                "A string literal cannot be used as an imported binding.\n- Did you mean `import \
                 {{ \"{s}\" as foo }}`?"
            )
            .into(),

            SyntaxError::ExportBindingIsString => {
                "A string literal cannot be used as an exported binding without `from`.".into()
            }

            SyntaxError::ConstDeclarationsRequireInitialization => {
                "'const' declarations must be initialized".into()
            }

            SyntaxError::DuplicatedRegExpFlags(flag) => {
                format!("Duplicated regular expression flag '{flag}'.").into()
            }
            SyntaxError::UnknownRegExpFlags => "Unknown regular expression flags.".into(),

            SyntaxError::TS1003 => "Expected an identifier".into(),
            SyntaxError::TS1005 => "Expected a semicolon".into(),
            SyntaxError::TS1009 => "Trailing comma is not allowed".into(),
            SyntaxError::TS1014 => "A rest parameter must be last in a parameter list".into(),
            SyntaxError::TS1015 => "Parameter cannot have question mark and initializer".into(),
            SyntaxError::TS1029(left, right) => {
                format!("'{left}' modifier must precede '{right}' modifier.").into()
            }
            SyntaxError::TS1030(word) => format!("'{word}' modifier already seen.").into(),
            SyntaxError::TS1031 => {
                "`declare` modifier cannot appear on class elements of this kind".into()
            }
            SyntaxError::TS1038 => {
                "`declare` modifier not allowed for code already in an ambient context".into()
            }
            SyntaxError::TS1042 => "`async` modifier cannot be used here".into(),
            SyntaxError::TS1047 => "A rest parameter cannot be optional".into(),
            SyntaxError::TS1048 => "A rest parameter cannot have an initializer".into(),
            SyntaxError::TS1085 => "Legacy octal literals are not available when targeting \
                                    ECMAScript 5 and higher"
                .into(),
            SyntaxError::TS1089(word) => {
                format!("'{word}' modifier cannot appear on a constructor declaration").into()
            }
            SyntaxError::TS1092 => {
                "Type parameters cannot appear on a constructor declaration".into()
            }
            SyntaxError::TS1096 => "An index signature must have exactly one parameter".into(),
            SyntaxError::TS1098 => "Type parameter list cannot be empty".into(),
            SyntaxError::TS1100 => "Invalid use of 'arguments' in strict mode".into(),
            SyntaxError::TS1102 => {
                "'delete' cannot be called on an identifier in strict mode".into()
            }
            SyntaxError::TS1105 => "A 'break' statement can only be used within an enclosing \
                                    iteration or switch statement"
                .into(),
            SyntaxError::TS1106 => {
                "The left-hand side of a `for...of` statement may not be `async`".into()
            }
            SyntaxError::TS1107 => "Jump target cannot cross function boundary".into(),
            SyntaxError::TS1109 => "Expression expected".into(),
            SyntaxError::TS1114 => "Duplicate label".into(),
            SyntaxError::TS1115 => "A 'continue' statement can only jump to a label of an \
                                    enclosing iteration statement"
                .into(),
            SyntaxError::TS1116 => {
                "A 'break' statement can only jump to a label of an enclosing statement".into()
            }
            SyntaxError::TS1123 => "Variable declaration list cannot be empty".into(),
            SyntaxError::TS1162 => "An object member cannot be declared optional".into(),
            SyntaxError::TS1164 => "Computed property names are not allowed in enums".into(),
            SyntaxError::TS1171 => {
                "A comma expression is not allowed in a computed property name".into()
            }
            SyntaxError::TS1172 => "`extends` clause already seen.".into(),
            SyntaxError::TS1173 => "'extends' clause must precede 'implements' clause.".into(),
            SyntaxError::TS1174 => "Classes can only extend a single class".into(),
            SyntaxError::TS1175 => "`implements` clause already seen".into(),
            SyntaxError::TS1183 => {
                "An implementation cannot be declared in ambient contexts".into()
            }
            SyntaxError::TS1184 => "Modifiers cannot appear here".into(),
            SyntaxError::TS1185 => "Merge conflict marker encountered.".into(),
            SyntaxError::TS1093 => {
                "Type annotation cannot appear on a constructor declaration".into()
            }
            SyntaxError::TS1196 => "Catch clause variable cannot have a type annotation".into(),
            SyntaxError::TS1242 => {
                "`abstract` modifier can only appear on a class or method declaration".into()
            }
            SyntaxError::TS1244 => {
                "Abstract methods can only appear within an abstract class.".into()
            }
            SyntaxError::TS1243(left, right) => {
                format!("'{left}' modifier cannot be used with '{right}' modifier.").into()
            }
            SyntaxError::TS1245 => "Abstract method cannot have an implementation.".into(),
            SyntaxError::TS1267 => "Abstract property cannot have an initializer.".into(),
            SyntaxError::TS1273(word) => {
                format!("'{word}' modifier cannot appear on a type parameter").into()
            }
            SyntaxError::TS1274(word) => format!(
                "'{word}' modifier can only appear on a type parameter of a class, interface or \
                 type alias"
            )
            .into(),
            SyntaxError::TS1277(word) => format!(
                "'{word}' modifier can only appear on a type parameter of a function, method or \
                 class"
            )
            .into(),
            SyntaxError::TS2206 => "The 'type' modifier cannot be used on a named import when \
                                    'import type' is used on its import statement."
                .into(),
            SyntaxError::TS2207 => "The 'type' modifier cannot be used on a named export when \
                                    'export type' is used on its export statement."
                .into(),
            SyntaxError::TS2369 => {
                "A parameter property is only allowed in a constructor implementation".into()
            }
            SyntaxError::TS2371 => "A parameter initializer is only allowed in a function or \
                                    constructor implementation"
                .into(),
            SyntaxError::TS2406 => "The left-hand side of an assignment expression must be a \
                                    variable or a property access."
                .into(),
            SyntaxError::TS2410 => "The 'with' statement is not supported. All symbols in a \
                                    'with' block will have type 'any'."
                .into(),
            SyntaxError::TS2414 => "Invalid class name".into(),
            SyntaxError::TS2427 => "interface name is invalid".into(),
            SyntaxError::TS2452 => "An enum member cannot have a numeric name".into(),
            SyntaxError::TS2483 => {
                "The left-hand side of a 'for...of' statement cannot use a type annotation".into()
            }
            SyntaxError::TS2491 => "The left-hand side of a 'for...in' statement cannot be a \
                                    destructuring pattern"
                .into(),
            SyntaxError::TS2499 => "An interface can only extend an identifier/qualified-name \
                                    with optional type arguments."
                .into(),
            SyntaxError::TS4112 => "This member cannot have an 'override' modifier because its \
                                    containing class does not extend another class."
                .into(),
            SyntaxError::TS8038 => "Decorators may not appear after `export` or `export default` \
                                    if they also appear before `export`."
                .into(),
            SyntaxError::TS18010 => {
                "An accessibility modifier cannot be used with a private identifier.".into()
            }
            SyntaxError::TSTypeAnnotationAfterAssign => {
                "Type annotations must come before default assignments".into()
            }
            SyntaxError::TsNonNullAssertionNotAllowed(word) => {
                format!("Typescript non-null assertion operator is not allowed with '{word}'")
                    .into()
            }
            SyntaxError::SetterParamRequired => "Setter should have exactly one parameter".into(),
            SyntaxError::UnexpectedTokenWithSuggestions {
                candidate_list: token_list,
            } => {
                let did_you_mean = if token_list.len() <= 2 {
                    token_list.join(" or ")
                } else {
                    token_list[0..token_list.len() - 1].join(" , ")
                        + &*format!("or {}", token_list[token_list.len() - 1])
                };
                format!("Unexpected token. Did you mean {did_you_mean}?").into()
            }
            SyntaxError::WithLabel { inner, .. } => inner.error.1.msg(),
            SyntaxError::ReservedTypeAssertion => "This syntax is reserved in files with the .mts \
                                                   or .cts extension. Use an `as` expression \
                                                   instead."
                .into(),
            SyntaxError::ReservedArrowTypeParam => "This syntax is reserved in files with the \
                                                    .mts or .cts extension. Add a trailing comma, \
                                                    as in `<T,>() => ...`."
                .into(),
            SyntaxError::InvalidAssignTarget => "Invalid assignment target".into(),
            SyntaxError::EmptyTypeArgumentList => "Type argument list cannot be empty.".into(),
        }
    }
}

impl Error {
    #[cold]
    #[inline(never)]
    pub fn into_diagnostic(self, handler: &Handler) -> DiagnosticBuilder {
        if let SyntaxError::WithLabel { inner, note, span } = self.error.1 {
            let mut db = inner.into_diagnostic(handler);
            db.span_label(span, note);
            return db;
        }

        let span = self.span();

        let kind = self.into_kind();
        let msg = kind.msg();

        let mut db = handler.struct_span_err(span, &msg);

        match kind {
            SyntaxError::ExpectedSemiForExprStmt { expr } => {
                db.span_label(
                    expr,
                    "This is the expression part of an expression statement",
                );
            }
            SyntaxError::MultipleDefault { previous } => {
                db.span_label(previous, "previous default case is declared at here");
            }
            _ => {}
        }

        db
    }
}

#[test]
fn size_of_error() {
    assert_eq!(std::mem::size_of::<Error>(), 8);
}
