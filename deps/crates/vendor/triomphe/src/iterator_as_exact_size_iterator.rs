/// Wrap an iterator and implement `ExactSizeIterator`
/// assuming the underlying iterator reports lower bound equal to upper bound.
///
/// It does not check the size is reported correctly (except in debug mode).
pub(crate) struct IteratorAsExactSizeIterator<I> {
    iter: I,
}

impl<I: Iterator> IteratorAsExactSizeIterator<I> {
    #[inline]
    pub(crate) fn new(iter: I) -> Self {
        let (lower, upper) = iter.size_hint();
        debug_assert_eq!(
            Some(lower),
            upper,
            "IteratorAsExactSizeIterator requires size hint lower == upper"
        );
        IteratorAsExactSizeIterator { iter }
    }
}

impl<I: Iterator> Iterator for IteratorAsExactSizeIterator<I> {
    type Item = I::Item;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next()
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<I: Iterator> ExactSizeIterator for IteratorAsExactSizeIterator<I> {
    #[inline]
    fn len(&self) -> usize {
        let (lower, upper) = self.iter.size_hint();
        debug_assert_eq!(
            Some(lower),
            upper,
            "IteratorAsExactSizeIterator requires size hint lower == upper"
        );
        lower
    }
}
