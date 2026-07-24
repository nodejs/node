// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This data is confirmed by both the [Korea Astronomy and Space Science Institute](https://astro.kasi.re.kr/life/pageView/5),
//! as well as the [Hong Kong Observatory](https://www.hko.gov.hk/en/gts/time/conversion.htm).

use super::PackedEastAsianTraditionalYearData;

pub const STARTING_YEAR: i32 = 1900;

#[rustfmt::skip]
#[allow(clippy::unwrap_used)] // const
pub const DATA: &[PackedEastAsianTraditionalYearData] = {
    use calendrical_calculations::gregorian::fixed_from_gregorian as gregorian;
    let l = true; // long
    let s = false; // short
    &[
        PackedEastAsianTraditionalYearData::new(1900, [s, l, s, s, l, s, l, l, s, l, l, s, l], Some(9), gregorian(1900, 1, 31)),
        PackedEastAsianTraditionalYearData::new(1901, [s, l, s, s, l, s, l, s, l, l, l, s, s], None, gregorian(1901, 2, 19)),
        PackedEastAsianTraditionalYearData::new(1902, [l, s, l, s, s, l, s, l, s, l, l, l, s], None, gregorian(1902, 2, 8)),
        PackedEastAsianTraditionalYearData::new(1903, [s, l, s, l, s, s, l, s, s, l, l, s, l], Some(6), gregorian(1903, 1, 29)),
        PackedEastAsianTraditionalYearData::new(1904, [l, l, s, l, s, s, l, s, s, l, l, s, s], None, gregorian(1904, 2, 16)),
        PackedEastAsianTraditionalYearData::new(1905, [l, l, s, l, l, s, s, l, s, l, s, l, s], None, gregorian(1905, 2, 4)),
        PackedEastAsianTraditionalYearData::new(1906, [s, l, l, s, l, s, l, s, l, s, l, s, l], Some(5), gregorian(1906, 1, 25)),
        PackedEastAsianTraditionalYearData::new(1907, [s, l, s, l, s, l, l, s, l, s, l, s, s], None, gregorian(1907, 2, 13)),
        PackedEastAsianTraditionalYearData::new(1908, [l, s, s, l, l, s, l, s, l, l, s, l, s], None, gregorian(1908, 2, 2)),
        PackedEastAsianTraditionalYearData::new(1909, [s, l, s, s, l, s, l, s, l, l, l, s, l], Some(3), gregorian(1909, 1, 22)),
        PackedEastAsianTraditionalYearData::new(1910, [s, l, s, s, l, s, l, s, l, l, l, s, s], None, gregorian(1910, 2, 10)),
        PackedEastAsianTraditionalYearData::new(1911, [l, s, l, s, s, l, s, s, l, l, s, l, l], Some(7), gregorian(1911, 1, 30)),
    ]
};
