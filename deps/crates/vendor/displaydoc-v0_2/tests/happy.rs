use displaydoc::Display;

#[cfg(feature = "std")]
use std::path::PathBuf;

#[derive(Display)]
/// Just a basic struct {thing}
struct HappyStruct {
    thing: &'static str,
}

#[derive(Display)]
#[ignore_extra_doc_attributes]
/// Just a basic struct {thing}
/// and this line should not get ignored
struct HappyStruct2 {
    thing: &'static str,
}

#[derive(Display)]
/// Really fancy first line with thing: {thing}
/// Really cool second line
struct HappyMultiLine {
    thing: &'static str,
}

#[derive(Display)]
#[ignore_extra_doc_attributes]
/// multi
/// line
///
/// new paragraph should be ignored
struct HappyMultilineWithIgnore;

#[derive(Display)]
enum MixedBlockAndLineComments {
    /**
     * hello
     * block
     * comment
     */
    /// line comment
    BlockFirst,
    /// line comment
    /**
     * hello
     * block
     * comment
     */
    LineFirst,
    /**
     * block
     * comment
     */
    /**
     * block
     * comment2
     */
    DoubleBlock,
}

#[derive(Display)]
enum Happy {
    /// I really like Variant1
    Variant1,
    /// Variant2 is pretty swell 2
    Variant2,
    /// Variant3 is okay {sometimes}
    Variant3 { sometimes: &'static str },
    /**
     * Variant4 wants to have a lot of lines
     *
     * Lets see how this works out for it
     */
    Variant4,
    /// Variant5 has a parameter {0} and some regular comments
    // A regular comment that won't get picked
    Variant5(u32),

    /// The path {0}
    #[cfg(feature = "std")]
    Variant6(PathBuf),

    /// These docs are ignored
    #[displaydoc("Variant7 has a parameter {0} and uses #[displaydoc]")]
    /// These docs are also ignored
    Variant7(u32),
}

// Used for testing indented doc comments
mod inner_mod {
    use super::Display;

    #[derive(Display)]
    pub enum InnerHappy {
        /// I really like Variant1
        Variant1,
        /// Variant2 is pretty swell 2
        Variant2,
        /// Variant3 is okay {sometimes}
        Variant3 { sometimes: &'static str },
        /**
         * Variant4 wants to have a lot of lines
         *
         * Lets see how this works out for it
         */
        Variant4,
        /// Variant5 has a parameter {0} and some regular comments
        // A regular comment that won't get picked
        Variant5(u32),

        /** what happens if we
         * put text on the first line?
         */
        Variant6,

        /**
        what happens if we don't use *?
        */
        Variant7,

        /**
         *
         * what about extra new lines?
         */
        Variant8,
    }
}

fn assert_display<T: std::fmt::Display>(input: T, expected: &'static str) {
    let out = format!("{}", input);
    assert_eq!(expected, out);
}

#[test]
fn does_it_print() {
    assert_display(Happy::Variant1, "I really like Variant1");
    assert_display(Happy::Variant2, "Variant2 is pretty swell 2");
    assert_display(Happy::Variant3 { sometimes: "hi" }, "Variant3 is okay hi");
    assert_display(
        Happy::Variant4,
        "Variant4 wants to have a lot of lines\n\nLets see how this works out for it",
    );
    assert_display(
        Happy::Variant5(2),
        "Variant5 has a parameter 2 and some regular comments",
    );
    assert_display(
        Happy::Variant7(2),
        "Variant7 has a parameter 2 and uses #[displaydoc]",
    );
    assert_display(HappyStruct { thing: "hi" }, "Just a basic struct hi");

    assert_display(
        HappyStruct2 { thing: "hi2" },
        "Just a basic struct hi2 and this line should not get ignored",
    );

    assert_display(
        HappyMultiLine { thing: "rust" },
        "Really fancy first line with thing: rust Really cool second line",
    );

    assert_display(HappyMultilineWithIgnore, "multi line");

    assert_display(
        MixedBlockAndLineComments::BlockFirst,
        "hello\nblock\ncomment line comment",
    );
    assert_display(
        MixedBlockAndLineComments::LineFirst,
        "line comment hello\nblock\ncomment",
    );
    assert_display(
        MixedBlockAndLineComments::DoubleBlock,
        "block\ncomment block\ncomment2",
    );

    assert_display(inner_mod::InnerHappy::Variant1, "I really like Variant1");
    assert_display(
        inner_mod::InnerHappy::Variant2,
        "Variant2 is pretty swell 2",
    );
    assert_display(
        inner_mod::InnerHappy::Variant3 { sometimes: "hi" },
        "Variant3 is okay hi",
    );
    assert_display(
        inner_mod::InnerHappy::Variant4,
        "Variant4 wants to have a lot of lines\n\nLets see how this works out for it",
    );
    assert_display(
        inner_mod::InnerHappy::Variant5(2),
        "Variant5 has a parameter 2 and some regular comments",
    );
    assert_display(
        inner_mod::InnerHappy::Variant6,
        "what happens if we\nput text on the first line?",
    );
    assert_display(
        inner_mod::InnerHappy::Variant7,
        "what happens if we don\'t use *?",
    );
    assert_display(
        inner_mod::InnerHappy::Variant8,
        "what about extra new lines?",
    );
}

#[test]
#[cfg(feature = "std")]
fn does_it_print_path() {
    assert_display(
        Happy::Variant6(PathBuf::from("/var/log/happy")),
        "The path /var/log/happy",
    );
}
