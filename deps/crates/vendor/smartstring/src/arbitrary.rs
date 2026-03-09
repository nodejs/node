// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{SmartString, SmartStringMode};
use alloc::string::String;
use arbitrary::{Arbitrary, Result, Unstructured};

impl<'a, Mode: SmartStringMode> Arbitrary<'a> for SmartString<Mode>
where
    Mode: 'static,
{
    fn arbitrary(u: &mut Unstructured<'_>) -> Result<Self> {
        String::arbitrary(u).map(Self::from)
    }

    fn arbitrary_take_rest(u: Unstructured<'_>) -> Result<Self> {
        String::arbitrary_take_rest(u).map(Self::from)
    }

    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        String::size_hint(depth)
    }
}
