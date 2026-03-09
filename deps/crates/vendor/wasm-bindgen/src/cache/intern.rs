use cfg_if::cfg_if;

cfg_if! {
    if #[cfg(feature = "enable-interning")] {
        use std::thread_local;
        use std::string::String;
        use std::borrow::ToOwned;
        use std::cell::RefCell;
        use std::collections::HashMap;
        use crate::JsValue;

        struct Cache {
            entries: RefCell<HashMap<String, JsValue>>,
        }

        thread_local! {
            static CACHE: Cache = Cache {
                entries: RefCell::new(HashMap::new()),
            };
        }

        /// This returns the raw index of the cached JsValue, so you must take care
        /// so that you don't use it after it is freed.
        pub(crate) fn unsafe_get_str(s: &str) -> Option<u32> {
            CACHE.with(|cache| {
                let cache = cache.entries.borrow();

                cache.get(s).map(|x| x.idx)
            })
        }

        fn intern_str(key: &str) {
            CACHE.with(|cache| {
                let entries = &cache.entries;

                // Can't use `entry` because `entry` requires a `String`
                if !entries.borrow().contains_key(key) {
                    // Note: we must not hold the borrow while we create the `JsValue`,
                    // because it will try to look up the value in the cache first.
                    let value = JsValue::from(key);
                    entries.borrow_mut().insert(key.to_owned(), value);
                }
            })
        }

        fn unintern_str(key: &str) {
            CACHE.with(|cache| {
                let mut cache = cache.entries.borrow_mut();

                cache.remove(key);
            })
        }
    }
}

/// Interns Rust strings so that it's much faster to send them to JS.
///
/// Sending strings from Rust to JS is slow, because it has to do a full `O(n)`
/// copy and *also* encode from UTF-8 to UTF-16. This must be done every single
/// time a string is sent to JS.
///
/// If you are sending the same string multiple times, you can call this `intern`
/// function, which simply returns its argument unchanged:
///
/// ```rust
/// # use wasm_bindgen::intern;
/// intern("foo") // returns "foo"
/// # ;
/// ```
///
/// However, if you enable the `"enable-interning"` feature for wasm-bindgen,
/// then it will add the string into an internal cache.
///
/// When you send that cached string to JS, it will look it up in the cache,
/// which completely avoids the `O(n)` copy and encoding. This has a significant
/// speed boost (as high as 783%)!
///
/// However, there is a small cost to this caching, so you shouldn't cache every
/// string. Only cache strings which have a high likelihood of being sent
/// to JS multiple times.
///
/// Also, keep in mind that this function is a *performance hint*: it's not
/// *guaranteed* that the string will be cached, and the caching strategy
/// might change at any time, so don't rely upon it.
#[inline]
pub fn intern(s: &str) -> &str {
    #[cfg(feature = "enable-interning")]
    intern_str(s);

    s
}

/// Removes a Rust string from the intern cache.
///
/// This does the opposite of the [`intern`](fn.intern.html) function.
///
/// If the [`intern`](fn.intern.html) function is called again then it will re-intern the string.
#[allow(unused_variables)]
#[inline]
pub fn unintern(s: &str) {
    #[cfg(feature = "enable-interning")]
    unintern_str(s);
}
