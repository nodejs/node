use std::error::Error;

use regex_automata::{
    dfa::{dense, Automaton, OverlappingState},
    nfa::thompson,
    Anchored, HalfMatch, Input, MatchError,
};

// Tests that quit bytes in the forward direction work correctly.
#[test]
fn quit_fwd() -> Result<(), Box<dyn Error>> {
    let dfa = dense::Builder::new()
        .configure(dense::Config::new().quit(b'x', true))
        .build("[[:word:]]+$")?;

    assert_eq!(
        Err(MatchError::quit(b'x', 3)),
        dfa.try_search_fwd(&Input::new(b"abcxyz"))
    );
    assert_eq!(
        dfa.try_search_overlapping_fwd(
            &Input::new(b"abcxyz"),
            &mut OverlappingState::start()
        ),
        Err(MatchError::quit(b'x', 3)),
    );

    Ok(())
}

// Tests that quit bytes in the reverse direction work correctly.
#[test]
fn quit_rev() -> Result<(), Box<dyn Error>> {
    let dfa = dense::Builder::new()
        .configure(dense::Config::new().quit(b'x', true))
        .thompson(thompson::Config::new().reverse(true))
        .build("^[[:word:]]+")?;

    assert_eq!(
        Err(MatchError::quit(b'x', 3)),
        dfa.try_search_rev(&Input::new(b"abcxyz"))
    );

    Ok(())
}

// Tests that if we heuristically enable Unicode word boundaries but then
// instruct that a non-ASCII byte should NOT be a quit byte, then the builder
// will panic.
#[test]
#[should_panic]
fn quit_panics() {
    dense::Config::new().unicode_word_boundary(true).quit(b'\xFF', false);
}

// This tests an intesting case where even if the Unicode word boundary option
// is disabled, setting all non-ASCII bytes to be quit bytes will cause Unicode
// word boundaries to be enabled.
#[test]
fn unicode_word_implicitly_works() -> Result<(), Box<dyn Error>> {
    let mut config = dense::Config::new();
    for b in 0x80..=0xFF {
        config = config.quit(b, true);
    }
    let dfa = dense::Builder::new().configure(config).build(r"\b")?;
    let expected = HalfMatch::must(0, 1);
    assert_eq!(Ok(Some(expected)), dfa.try_search_fwd(&Input::new(b" a")));
    Ok(())
}

// A variant of [`Automaton::is_special_state`]'s doctest, but with universal
// start states.
//
// See: https://github.com/rust-lang/regex/pull/1195
#[test]
fn universal_start_search() -> Result<(), Box<dyn Error>> {
    fn find<A: Automaton>(
        dfa: &A,
        haystack: &[u8],
    ) -> Result<Option<HalfMatch>, MatchError> {
        let mut state = dfa
            .universal_start_state(Anchored::No)
            .expect("regex should not require lookbehind");
        let mut last_match = None;
        // Walk all the bytes in the haystack. We can quit early if we see
        // a dead or a quit state. The former means the automaton will
        // never transition to any other state. The latter means that the
        // automaton entered a condition in which its search failed.
        for (i, &b) in haystack.iter().enumerate() {
            state = dfa.next_state(state, b);
            if dfa.is_special_state(state) {
                if dfa.is_match_state(state) {
                    last_match =
                        Some(HalfMatch::new(dfa.match_pattern(state, 0), i));
                } else if dfa.is_dead_state(state) {
                    return Ok(last_match);
                } else if dfa.is_quit_state(state) {
                    // It is possible to enter into a quit state after
                    // observing a match has occurred. In that case, we
                    // should return the match instead of an error.
                    if last_match.is_some() {
                        return Ok(last_match);
                    }
                    return Err(MatchError::quit(b, i));
                }
                // Implementors may also want to check for start or accel
                // states and handle them differently for performance
                // reasons. But it is not necessary for correctness.
            }
        }
        // Matches are always delayed by 1 byte, so we must explicitly walk
        // the special "EOI" transition at the end of the search.
        state = dfa.next_eoi_state(state);
        if dfa.is_match_state(state) {
            last_match = Some(HalfMatch::new(
                dfa.match_pattern(state, 0),
                haystack.len(),
            ));
        }
        Ok(last_match)
    }

    fn check_impl(
        dfa: impl Automaton,
        haystack: &str,
        pat: usize,
        offset: usize,
    ) -> Result<(), Box<dyn Error>> {
        let haystack = haystack.as_bytes();
        let mat = find(&dfa, haystack)?.unwrap();
        assert_eq!(mat.pattern().as_usize(), pat);
        assert_eq!(mat.offset(), offset);
        Ok(())
    }

    fn check(
        dfa: &dense::DFA<Vec<u32>>,
        haystack: &str,
        pat: usize,
        offset: usize,
    ) -> Result<(), Box<dyn Error>> {
        check_impl(dfa, haystack, pat, offset)?;
        check_impl(dfa.to_sparse()?, haystack, pat, offset)?;
        Ok(())
    }

    let dfa = dense::DFA::new(r"[a-z]+")?;
    let haystack = "123 foobar 4567";
    check(&dfa, haystack, 0, 10)?;

    let dfa = dense::DFA::new(r"[0-9]{4}")?;
    let haystack = "123 foobar 4567";
    check(&dfa, haystack, 0, 15)?;

    let dfa = dense::DFA::new_many(&[r"[a-z]+", r"[0-9]+"])?;
    let haystack = "123 foobar 4567";
    check(&dfa, haystack, 1, 3)?;
    check(&dfa, &haystack[3..], 0, 7)?;
    check(&dfa, &haystack[10..], 1, 5)?;

    Ok(())
}
