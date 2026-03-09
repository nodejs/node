#![allow(
    clippy::derive_partial_eq_without_eq,
    clippy::identity_op,
    clippy::let_underscore_untyped,
    clippy::shadow_unrelated
)]

use seq_macro::seq;

seq!(N in 0..8 {
    // nothing
});

#[test]
fn test_nothing() {
    macro_rules! expand_to_nothing {
        ($arg:literal) => {
            // nothing
        };
    }

    seq!(N in 0..4 {
        expand_to_nothing!(N);
    });
}

#[test]
fn test_fn() {
    seq!(N in 1..4 {
        fn f~N () -> u64 {
            N * 2
        }
    });

    // This f0 is written separately to detect whether seq correctly starts with
    // the first iteration at N=1 as specified in the invocation. If the macro
    // incorrectly started at N=0, the first generated function would conflict
    // with this one and the program would not compile.
    fn f0() -> u64 {
        100
    }

    let sum = f0() + f1() + f2() + f3();
    assert_eq!(sum, 100 + 2 + 4 + 6);
}

#[test]
fn test_stringify() {
    let strings = seq!(N in 9..12 {
        [
            #(
                stringify!(N),
            )*
        ]
    });
    assert_eq!(strings, ["9", "10", "11"]);
}

#[test]
fn test_underscores() {
    let n = seq!(N in 100_000..100_001 { N });
    assert_eq!(100_000, n);
}

#[test]
fn test_suffixed() {
    let n = seq!(N in 0..1u16 { stringify!(N) });
    assert_eq!(n, "0u16");
}

#[test]
fn test_padding() {
    seq!(N in 098..=100 {
        fn e~N() -> &'static str {
            stringify!(N)
        }
    });
    let strings = [e098(), e099(), e100()];
    assert_eq!(strings, ["098", "099", "100"]);
}

#[test]
fn test_byte() {
    seq!(c in b'x'..=b'z' {
        fn get_~c() -> u8 {
            c
        }
    });
    let bytes = [get_x(), get_y(), get_z()];
    assert_eq!(bytes, *b"xyz");
}

#[test]
fn test_char() {
    seq!(ch in 'x'..='z' {
        fn get_~ch() -> char {
            ch
        }
    });
    let chars = [get_x(), get_y(), get_z()];
    assert_eq!(chars, ['x', 'y', 'z']);
}

#[test]
fn test_binary() {
    let s = seq!(B in 0b00..=0b11 { stringify!(#(B)*) });
    let expected = "0b00 0b01 0b10 0b11";
    assert_eq!(expected, s);
}

#[test]
fn test_octal() {
    let s = seq!(O in 0o6..0o12 { stringify!(#(O)*) });
    let expected = "0o6 0o7 0o10 0o11";
    assert_eq!(expected, s);
}

#[test]
fn test_hex() {
    let s = seq!(X in 0x08..0x0c { stringify!(#(X)*) });
    let expected = "0x08 0x09 0x0a 0x0b";
    assert_eq!(expected, s);

    let s = seq!(X in 0x08..0x0C { stringify!(#(X)*) });
    let expected = "0x08 0x09 0x0A 0x0B";
    assert_eq!(expected, s);

    let s = seq!(X in 0X09..0X10 { stringify!(#(X)*) });
    let expected = "0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F";
    assert_eq!(expected, s);
}

#[test]
fn test_radix_concat() {
    seq!(B in 0b011..0b101 { struct S~B; });
    seq!(O in 0o007..0o011 { struct S~O; });
    seq!(X in 0x00a..0x00c { struct S~X; });
    seq!(X in 0x00C..0x00E { struct S~X; });
    let _ = (S011, S100, S007, S010, S00a, S00b, S00C, S00D);
}

#[test]
fn test_ident() {
    macro_rules! create {
        ($prefix:ident) => {
            seq!(N in 0..1 {
                struct $prefix~N;
            });
        };
    }
    create!(Pin);
    let _ = Pin0;
}

pub mod test_enum {
    use seq_macro::seq;

    seq!(N in 0..16 {
        #[derive(Copy, Clone, PartialEq, Debug)]
        pub enum Interrupt {
            #(
                Irq~N,
            )*
        }
    });

    #[test]
    fn test() {
        let interrupt = Interrupt::Irq8;
        assert_eq!(interrupt as u8, 8);
        assert_eq!(interrupt, Interrupt::Irq8);
    }
}

pub mod test_inclusive {
    use seq_macro::seq;

    seq!(N in 16..=20 {
        pub enum E {
            #(
                Variant~N,
            )*
        }
    });

    #[test]
    fn test() {
        let e = E::Variant16;

        let desc = match e {
            E::Variant16 => "min",
            E::Variant17 | E::Variant18 | E::Variant19 => "in between",
            E::Variant20 => "max",
        };

        assert_eq!(desc, "min");
    }
}

#[test]
fn test_array() {
    const PROCS: [Proc; 256] = {
        seq!(N in 0..256 {
            [
                #(
                    Proc::new(N),
                )*
            ]
        })
    };

    struct Proc {
        id: usize,
    }

    impl Proc {
        const fn new(id: usize) -> Self {
            Proc { id }
        }
    }

    assert_eq!(PROCS[32].id, 32);
}

pub mod test_group {
    use seq_macro::seq;

    // Source of truth. Call a given macro passing nproc as argument.
    macro_rules! pass_nproc {
        ($mac:ident) => {
            $mac! { 256 }
        };
    }

    macro_rules! literal_identity_macro {
        ($nproc:literal) => {
            $nproc
        };
    }

    const NPROC: usize = pass_nproc!(literal_identity_macro);

    pub struct Proc;

    impl Proc {
        const fn new() -> Self {
            Proc
        }
    }

    pub struct Mutex<T: ?Sized>(T);

    impl<T> Mutex<T> {
        const fn new(_name: &'static str, value: T) -> Self {
            Mutex(value)
        }
    }

    macro_rules! make_procs_array {
        ($nproc:literal) => {
            seq!(N in 0..$nproc { [#(Proc::new(),)*] })
        }
    }

    pub static PROCS: Mutex<[Proc; NPROC]> = Mutex::new("procs", pass_nproc!(make_procs_array));
}

#[test]
fn test_nested() {
    let mut vec = Vec::new();
    macro_rules! some_macro {
        ($($t:ident,)*) => {
            vec.push(stringify!($($t)*));
        };
    }

    seq!(I in 1..=3 {
        #(
            seq!(J in 1..=I {
                some_macro!(
                    #(T~J,)*
                );
            });
        )*
    });

    assert_eq!(vec, ["T1", "T1 T2", "T1 T2 T3"]);
}
