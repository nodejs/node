use crate::digit_table;
use core::mem;

const _: () = {
    let mut static_data = mem::size_of_val(&digit_table::DIGIT_TABLE);

    #[cfg(feature = "small")]
    {
        use crate::d2s_small_table;

        static_data += mem::size_of_val(&d2s_small_table::DOUBLE_POW5_INV_SPLIT2)
            + mem::size_of_val(&d2s_small_table::POW5_INV_OFFSETS)
            + mem::size_of_val(&d2s_small_table::DOUBLE_POW5_SPLIT2)
            + mem::size_of_val(&d2s_small_table::POW5_OFFSETS)
            + mem::size_of_val(&d2s_small_table::DOUBLE_POW5_TABLE);

        assert!(static_data == 1016);
    }

    #[cfg(not(feature = "small"))]
    {
        use crate::d2s_full_table;

        static_data += mem::size_of_val(&d2s_full_table::DOUBLE_POW5_INV_SPLIT)
            + mem::size_of_val(&d2s_full_table::DOUBLE_POW5_SPLIT);

        assert!(static_data == 10888); // 10.6K
    }
};
