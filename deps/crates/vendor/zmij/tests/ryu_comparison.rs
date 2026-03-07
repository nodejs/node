use rand::rngs::{SmallRng, SysRng};
use rand::{Rng as _, SeedableRng as _};

const N: usize = if cfg!(miri) {
    500
} else if let b"0" = opt_level::OPT_LEVEL.as_bytes() {
    1_000_000
} else {
    100_000_000
};

#[test]
fn ryu_comparison() {
    let mut ryu_buffer = ryu::Buffer::new();
    let mut zmij_buffer = zmij::Buffer::new();
    let mut rng = SmallRng::try_from_rng(&mut SysRng).unwrap();
    let mut fail = 0;

    for _ in 0..N {
        let bits = rng.next_u64();
        let float = f64::from_bits(bits);
        let ryu = ryu_buffer.format(float);
        let zmij = zmij_buffer.format(float);
        let matches = if ryu.contains('e') && !ryu.contains("e-") {
            ryu.split_once('e') == zmij.split_once("e+")
        } else {
            ryu == zmij
        };
        if !matches {
            eprintln!("RYU={ryu} ZMIJ={zmij}");
            fail += 1;
        }
    }

    assert!(fail == 0, "{fail} mismatches");
}
