use swc_common::{BytePos, Span, Spanned};

#[test]
fn lo_hi() {
    #[derive(Spanned)]
    struct LoHi {
        #[span(lo)]
        pub lo: BytePos,
        #[span(hi)]
        pub hi: BytePos,
    }

    let lo = BytePos(0);
    let hi = BytePos(5);
    assert_eq!(LoHi { lo, hi }.span(), Span::new(lo, hi,));
}
