#![no_std]
#![allow(unused)]

// This test ensures that the generated code doesn't reference any stdlib items.

use displaydoc::Display;

#[derive(Display)]
/// Just a basic struct {thing}
struct HappyStruct {
    thing: &'static str,
}

#[derive(Display)]
#[ignore_extra_doc_attributes]
/// Just a basic struct {thing}
/// and this line should get ignored
struct HappyStruct2 {
    thing: &'static str,
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

    /// These docs are ignored
    #[displaydoc("Variant7 has a parameter {0} and uses #[displaydoc]")]
    /// These docs are also ignored
    Variant7(u32),
}
