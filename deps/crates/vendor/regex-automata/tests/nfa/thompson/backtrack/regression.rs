// Tests that we can call the backtracker with more slots
// than is actually in the compiled regex.
#[test]
fn too_many_slots_normal_pattern() {
    use regex_automata::{
        nfa::thompson::backtrack::BoundedBacktracker,
        util::primitives::NonMaxUsize, Anchored, Input,
    };

    let expr = BoundedBacktracker::new(r"abc").unwrap();
    let s = "abc";
    let input = Input::new(s).span(0..s.len()).anchored(Anchored::Yes);

    let mut cache = expr.create_cache();
    let mut slots: Vec<Option<NonMaxUsize>> = vec![None; 4];
    let pid = expr.try_search_slots(&mut cache, &input, &mut slots).unwrap();
    assert_eq!(pid, Some(regex_automata::PatternID::must(0)));
    assert_eq!(slots[0], Some(NonMaxUsize::new(0).unwrap()));
    assert_eq!(slots[1], Some(NonMaxUsize::new(3).unwrap()));
}
