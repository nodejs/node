pub trait IteratorExt: Iterator {
    /// Copied from https://stackoverflow.com/a/49456265/6193633
    fn chain_with<F, I>(self, f: F) -> ChainWith<Self, F, I::IntoIter>
    where
        Self: Sized,
        F: FnOnce() -> I,
        I: IntoIterator<Item = Self::Item>,
    {
        ChainWith {
            base: self,
            factory: Some(f),
            iterator: None,
        }
    }
}

impl<I: Iterator> IteratorExt for I {}

pub struct ChainWith<B, F, I> {
    base: B,
    factory: Option<F>,
    iterator: Option<I>,
}

impl<B, F, I> Iterator for ChainWith<B, F, I::IntoIter>
where
    B: Iterator,
    F: FnOnce() -> I,
    I: IntoIterator<Item = B::Item>,
{
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(b) = self.base.next() {
            return Some(b);
        }

        // Exhausted the first, generate the second

        if let Some(f) = self.factory.take() {
            self.iterator = Some(f().into_iter());
        }

        self.iterator
            .as_mut()
            .expect("There must be an iterator")
            .next()
    }
}
