// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{boxed::BoxedString, inline::InlineString};

pub(crate) enum StringCast<'a> {
    Boxed(&'a BoxedString),
    Inline(&'a InlineString),
}

pub(crate) enum StringCastMut<'a> {
    Boxed(&'a mut BoxedString),
    Inline(&'a mut InlineString),
}

pub(crate) enum StringCastInto {
    Boxed(BoxedString),
    Inline(InlineString),
}
