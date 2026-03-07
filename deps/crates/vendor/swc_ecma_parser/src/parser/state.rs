use std::ops::{Deref, DerefMut};

use rustc_hash::FxHashMap;
use swc_atoms::Atom;
use swc_common::{BytePos, Span};

use crate::{input::Tokens, Parser};

#[derive(Clone, Default)]
pub struct State {
    pub labels: Vec<Atom>,
    /// Start position of an assignment expression.
    pub potential_arrow_start: Option<BytePos>,
    /// Start position of an AST node and the span of its trailing comma.
    pub trailing_commas: FxHashMap<BytePos, Span>,
}

pub struct WithState<'w, I: Tokens> {
    pub(super) inner: &'w mut Parser<I>,
    pub(super) orig_state: State,
}

impl<I: Tokens> Deref for WithState<'_, I> {
    type Target = Parser<I>;

    fn deref(&self) -> &Parser<I> {
        self.inner
    }
}
impl<I: Tokens> DerefMut for WithState<'_, I> {
    fn deref_mut(&mut self) -> &mut Parser<I> {
        self.inner
    }
}

impl<I: Tokens> Drop for WithState<'_, I> {
    fn drop(&mut self) {
        std::mem::swap(self.inner.state_mut(), &mut self.orig_state);
    }
}
