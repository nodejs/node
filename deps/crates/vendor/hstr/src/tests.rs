use crate::{wtf8::Wtf8, Atom, AtomStore, Wtf8Atom};

fn store_with_atoms(texts: Vec<&str>) -> (AtomStore, Vec<Atom>) {
    let mut store = AtomStore::default();

    let atoms = { texts.into_iter().map(|text| store.atom(text)).collect() };

    (store, atoms)
}

fn store_with_wtf8_atoms(texts: Vec<&Wtf8>) -> (AtomStore, Vec<Wtf8Atom>) {
    let mut store = AtomStore::default();

    let atoms = {
        texts
            .into_iter()
            .map(|text| store.wtf8_atom(text))
            .collect()
    };

    (store, atoms)
}

#[test]
fn simple_usage() {
    // atom
    let (s, atoms) = store_with_atoms(vec!["Hello, world!", "Hello, world!"]);

    drop(s);

    let a1 = atoms[0].clone();

    let a2 = atoms[1].clone();

    assert_eq!(a1.unsafe_data, a2.unsafe_data);

    // wtf8_atom
    let (s, atoms) = store_with_wtf8_atoms(vec![
        &Wtf8::from_str("Hello, world!"),
        &Wtf8::from_str("Hello, world!"),
    ]);

    drop(s);

    let a1 = atoms[0].clone();

    let a2 = atoms[1].clone();

    assert_eq!(a1.unsafe_data, a2.unsafe_data);
}

#[test]
fn eager_drop() {
    // atom
    let (_, atoms1) = store_with_atoms(vec!["Hello, world!!!!"]);
    let (_, atoms2) = store_with_atoms(vec!["Hello, world!!!!"]);

    dbg!(&atoms1);
    dbg!(&atoms2);

    let a1 = atoms1[0].clone();
    let a2 = atoms2[0].clone();

    assert_ne!(
        a1.unsafe_data, a2.unsafe_data,
        "Different stores should have different addresses"
    );
    assert_eq!(a1.get_hash(), a2.get_hash(), "Same string should be equal");
    assert_eq!(a1, a2, "Same string should be equal");

    // wtf8_atom
    let (_, atoms1) = store_with_wtf8_atoms(vec![&Wtf8::from_str("Hello, world!!!!")]);
    let (_, atoms2) = store_with_wtf8_atoms(vec![&Wtf8::from_str("Hello, world!!!!")]);

    dbg!(&atoms1);
    dbg!(&atoms2);

    let a1 = atoms1[0].clone();
    let a2 = atoms2[0].clone();

    assert_ne!(
        a1.unsafe_data, a2.unsafe_data,
        "Different stores should have different addresses"
    );
    assert_eq!(a1.get_hash(), a2.get_hash(), "Same string should be equal");
    assert_eq!(a1, a2, "Same string should be equal");
}

#[test]
fn store_multiple() {
    // atom
    let (_s1, atoms1) = store_with_atoms(vec!["Hello, world!!!!"]);
    let (_s2, atoms2) = store_with_atoms(vec!["Hello, world!!!!"]);

    let a1 = atoms1[0].clone();
    let a2 = atoms2[0].clone();

    assert_ne!(
        a1.unsafe_data, a2.unsafe_data,
        "Different stores should have different addresses"
    );
    assert_eq!(a1.get_hash(), a2.get_hash(), "Same string should be equal");
    assert_eq!(a1, a2, "Same string should be equal");

    // wtf8_atom
    let (_s1, atoms1) = store_with_wtf8_atoms(vec![&Wtf8::from_str("Hello, world!!!!")]);
    let (_s2, atoms2) = store_with_wtf8_atoms(vec![&Wtf8::from_str("Hello, world!!!!")]);

    let a1 = atoms1[0].clone();
    let a2 = atoms2[0].clone();

    assert_ne!(
        a1.unsafe_data, a2.unsafe_data,
        "Different stores should have different addresses"
    );
    assert_eq!(a1.get_hash(), a2.get_hash(), "Same string should be equal");
    assert_eq!(a1, a2, "Same string should be equal");
}

#[test]
fn store_ref_count() {
    // atom
    let (store, atoms) = store_with_atoms(vec!["Hello, world!!!!"]);

    assert_eq!(atoms[0].ref_count(), 2);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 1);

    // wtf8_atom
    let (store, atoms) = store_with_wtf8_atoms(vec![&Wtf8::from_str("Hello, world!!!!")]);

    assert_eq!(atoms[0].ref_count(), 2);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 1);
}

#[test]
fn store_ref_count_transitive() {
    // transitive &Atom -> Wtf8Atom
    let (store, atoms) = store_with_atoms(vec!["Hello, world!!!!"]);

    assert_eq!(atoms[0].ref_count(), 2);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 1);

    // Implicit clone here
    let wtf8 = Wtf8Atom::from(&atoms[0]);
    // Ref1 from atoms[0]
    // Ref2 from Wtf8Atom::from(&atoms[0])
    assert_eq!(wtf8.ref_count(), 2);
    drop(atoms);
    assert_eq!(wtf8.ref_count(), 1);

    // transitive Atom -> &Wtf8Atom
    let (store, atoms) = store_with_atoms(vec!["Hello, world!!!!"]);

    assert_eq!(atoms[0].ref_count(), 2);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 1);

    let wtf8 = Wtf8Atom::from(atoms[0].clone());
    assert_eq!(wtf8.ref_count(), 2);
    drop(atoms);
    assert_eq!(wtf8.ref_count(), 1);
}

#[test]
fn store_ref_count_transitive_roundtrip() {
    // transitive &Atom -> Wtf8Atom -> Atom
    let (store, atoms) = store_with_atoms(vec!["Hello, world!!!!"]);

    assert_eq!(atoms[0].ref_count(), 2);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 1);

    // Implicit clone here
    let wtf8 = Wtf8Atom::from(&atoms[0]);
    // Ref1 from atoms[0]
    // Ref2 from Wtf8Atom::from(&atoms[0])
    assert_eq!(wtf8.ref_count(), 2);

    let atom2 = unsafe { Atom::from_wtf8_unchecked(wtf8.clone()) };
    // Ref1 from atoms[0]
    // Ref2 from wtf8
    // Ref3 from atom2
    assert_eq!(atom2.ref_count(), 3);

    drop(atoms);
    assert_eq!(atom2.ref_count(), 2);

    drop(wtf8);
    assert_eq!(atom2.ref_count(), 1);
}

#[test]
fn store_ref_count_dynamic() {
    // atom
    let (store, atoms) = store_with_atoms(vec!["Hello, world!!!!"]);

    let a1 = atoms[0].clone();
    let a2 = atoms[0].clone();

    assert_eq!(atoms[0].ref_count(), 4);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 3);

    drop(a1);
    assert_eq!(atoms[0].ref_count(), 2);

    drop(a2);
    assert_eq!(atoms[0].ref_count(), 1);

    // wtf8_atom
    let (store, atoms) = store_with_wtf8_atoms(vec![&Wtf8::from_str("Hello, world!!!!")]);

    let a1 = atoms[0].clone();
    let a2 = atoms[0].clone();

    assert_eq!(atoms[0].ref_count(), 4);
    drop(store);
    assert_eq!(atoms[0].ref_count(), 3);

    drop(a1);
    assert_eq!(atoms[0].ref_count(), 2);

    drop(a2);
    assert_eq!(atoms[0].ref_count(), 1);
}

#[test]
fn wtf8_atom() {
    let s = "hello";
    let w = Wtf8Atom::from(s);
    assert_eq!(w, Atom::from(s));

    // Test enough to exceed the small string optimization
    let s = "abcdefghi";
    let w = Wtf8Atom::from(s);
    assert_eq!(w, Atom::from(s));
}
