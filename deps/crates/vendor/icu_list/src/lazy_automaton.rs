// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use regex_automata::dfa::sparse::DFA;
use regex_automata::dfa::Automaton;
use regex_automata::util::{primitives::StateID, start::Config as StartConfig};
use writeable::Writeable;

pub trait LazyAutomaton: Automaton {
    // Like Automaton::try_search_fwd, but doesn't require a materialized string.
    fn matches_earliest_fwd_lazy<S: Writeable + ?Sized>(&self, haystack: &S) -> bool;
}

impl<T: AsRef<[u8]>> LazyAutomaton for DFA<T> {
    fn matches_earliest_fwd_lazy<S: Writeable + ?Sized>(&self, haystack: &S) -> bool {
        struct DFAStepper<'a> {
            dfa: &'a DFA<&'a [u8]>,
            state: StateID,
        }

        impl core::fmt::Write for DFAStepper<'_> {
            fn write_str(&mut self, s: &str) -> core::fmt::Result {
                for &byte in s.as_bytes() {
                    self.state = self.dfa.next_state(self.state, byte);
                    if self.dfa.is_match_state(self.state) || self.dfa.is_dead_state(self.state) {
                        // We matched or are in a no-match-cycle, return early
                        return Err(core::fmt::Error);
                    }
                }
                Ok(())
            }
        }

        let Ok(start_state) =
            self.start_state(&StartConfig::new().anchored(regex_automata::Anchored::Yes))
        else {
            // This will not happen, because we're not using look-behind, and our DFA support anchored
            return false;
        };

        let mut stepper = DFAStepper {
            state: start_state,
            dfa: &self.as_ref(),
        };

        if haystack.write_to(&mut stepper).is_ok() {
            stepper.state = self.next_eoi_state(stepper.state);
        }

        self.is_match_state(stepper.state)
    }
}

#[cfg(test)]
#[test]
fn test() {
    use crate::provider::SerdeDFA;
    use regex_automata::Input;
    use std::borrow::Cow;

    let matcher = SerdeDFA::new(Cow::Borrowed("^11(000)*$")).unwrap();

    for writeable in [1i32, 11, 110, 11000, 211000] {
        assert_eq!(
            matcher
                .deref()
                .try_search_fwd(
                    &Input::new(writeable.write_to_string().as_bytes())
                        .anchored(regex_automata::Anchored::Yes)
                )
                .unwrap()
                .is_some(),
            matcher.deref().matches_earliest_fwd_lazy(&writeable)
        );
    }

    struct ExitEarlyTest;

    impl Writeable for ExitEarlyTest {
        fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
            sink.write_str("12")?;
            unreachable!()
        }
    }

    assert!(!matcher.deref().matches_earliest_fwd_lazy(&ExitEarlyTest));
}
