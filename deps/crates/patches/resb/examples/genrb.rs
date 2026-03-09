// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use resb::{binary, text};

fn main() {
    let input = include_str!("data/zoneinfo64.txt");

    let (in_bundle, keys_in_discovery_order) = match text::Reader::read(input) {
        Ok(result) => result,
        Err(err) => panic!("Failed to parse text bundle:\n{err}"),
    };

    let bytes = binary::Serializer::to_bytes(&in_bundle, &keys_in_discovery_order)
        .expect("Failed to generate binary bundle.");

    if bytes != include_bytes!("data/zoneinfo64.res") {
        // TODO: This does not currently generate a file that matches ICU4C's, and the file it generates
        // does not parse either.
        // std::fs::write(
        //     concat!(env!("CARGO_MANIFEST_DIR"), "/examples/data/zoneinfo64.res"),
        //     bytes,
        // )
        // .unwrap();
        panic!("zoneinfo64.res differs");
    }
}
