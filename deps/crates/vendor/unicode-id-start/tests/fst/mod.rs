#![allow(clippy::module_name_repetitions)]

pub fn id_start_fst() -> fst::Set<&'static [u8]> {
    let data = include_bytes!("id_start.fst");
    fst::Set::from(fst::raw::Fst::new(data.as_slice()).unwrap())
}

pub fn id_continue_fst() -> fst::Set<&'static [u8]> {
    let data = include_bytes!("id_continue.fst");
    fst::Set::from(fst::raw::Fst::new(data.as_slice()).unwrap())
}
