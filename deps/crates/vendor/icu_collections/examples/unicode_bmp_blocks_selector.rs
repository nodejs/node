// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// An example application which uses icu_uniset to test what blocks of
// Basic Multilingual Plane a character belongs to.
//
// In this example we use `CodePointInversionListBuilder` to construct just the first
// two blocks of the first plane, and use an instance of a `BMPBlockSelector`
// to retrieve which of those blocks each character of a string belongs to.
//
// This is a simple example of the API use and is severely oversimplified
// compared to real Unicode block selection.

#![no_main] // https://github.com/unicode-org/icu4x/issues/395
icu_benchmark_macros::instrument!();
use icu_benchmark_macros::println;

use icu_collections::codepointinvlist::{CodePointInversionList, CodePointInversionListBuilder};
use std::ops::RangeInclusive;

#[derive(Copy, Clone, Debug)]
enum BmpBlock {
    Basic,
    Latin1Supplement,
    Unknown,
}

const BLOCKS: [(BmpBlock, RangeInclusive<char>); 2] = [
    (BmpBlock::Basic, '\u{0000}'..='\u{007F}'),
    (BmpBlock::Latin1Supplement, '\u{0080}'..='\u{00FF}'),
];

struct BmpBlockSelector {
    blocks: [(BmpBlock, CodePointInversionList<'static>); 2],
}

impl BmpBlockSelector {
    pub fn new() -> BmpBlockSelector {
        BmpBlockSelector {
            blocks: BLOCKS.map(|(ch, range)| {
                (ch, {
                    let mut builder = CodePointInversionListBuilder::new();
                    builder.add_range(range);
                    builder.build()
                })
            }),
        }
    }

    pub fn select(&self, input: char) -> BmpBlock {
        for (block, set) in &self.blocks {
            if set.contains(input) {
                return *block;
            }
        }
        BmpBlock::Unknown
    }
}

fn main() {
    let selector = BmpBlockSelector::new();

    let sample = "Welcome to MyName©®, Алексей!";

    println!("\n====== Unicode BMP Block Selector example ============");
    for ch in sample.chars() {
        let block = selector.select(ch);
        println!("{ch}: {block:#?}");
    }
}
