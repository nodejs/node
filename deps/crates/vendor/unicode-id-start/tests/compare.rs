#![allow(
    clippy::incompatible_msrv, // https://github.com/rust-lang/rust-clippy/issues/12257
)]

mod fst;
mod roaring;
mod trie;

#[test]
fn compare_all_implementations() {
    let id_start_fst = fst::id_start_fst();
    let id_continue_fst = fst::id_continue_fst();
    let id_start_roaring = roaring::id_start_bitmap();
    let id_continue_roaring = roaring::id_continue_bitmap();

    for ch in '\0'..=char::MAX {
        let thought_to_be_start = unicode_id_start::is_id_start(ch);
        let thought_to_be_continue = unicode_id_start::is_id_continue(ch);

        // unicode-id
        assert_eq!(
            thought_to_be_start,
            unicode_id::UnicodeID::is_id_start(ch),
            "{ch:?}",
        );
        assert_eq!(
            thought_to_be_continue,
            unicode_id::UnicodeID::is_id_continue(ch),
            "{ch:?}",
        );

        // ucd-trie
        assert_eq!(
            thought_to_be_start,
            trie::ID_START.contains_char(ch),
            "{ch:?}",
        );
        assert_eq!(
            thought_to_be_continue,
            trie::ID_CONTINUE.contains_char(ch),
            "{ch:?}",
        );

        // fst
        assert_eq!(
            thought_to_be_start,
            id_start_fst.contains((ch as u32).to_be_bytes()),
            "{ch:?}",
        );
        assert_eq!(
            thought_to_be_continue,
            id_continue_fst.contains((ch as u32).to_be_bytes()),
            "{ch:?}",
        );

        // roaring
        assert_eq!(
            thought_to_be_start,
            id_start_roaring.contains(ch as u32),
            "{ch:?}",
        );
        assert_eq!(
            thought_to_be_continue,
            id_continue_roaring.contains(ch as u32),
            "{ch:?}",
        );
    }
}
