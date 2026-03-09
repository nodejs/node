use std::borrow::Cow;
use std::iter;

fn split_path(path: &str) -> Vec<&str> {
    let mut last_idx = 0;
    let mut rv = vec![];
    for (idx, _) in path.match_indices(&['/', '\\'][..]) {
        rv.push(&path[last_idx..idx]);
        last_idx = idx;
    }
    if last_idx < path.len() {
        rv.push(&path[last_idx..]);
    }
    rv
}

fn is_abs_path(s: &str) -> bool {
    if s.starts_with('/') {
        return true;
    } else if s.len() > 3 {
        let b = s.as_bytes();
        if b[1] == b':'
            && (b[2] == b'/' || b[2] == b'\\')
            && ((b[0] >= b'a' && b[0] <= b'z') || (b[0] >= b'A' && b[0] <= b'Z'))
        {
            return true;
        }
    }
    false
}

fn find_common_prefix_of_sorted_vec<'a>(items: &'a [Cow<'a, [&'a str]>]) -> Option<&'a [&'a str]> {
    if items.is_empty() {
        return None;
    }

    let shortest = &items[0];
    let mut max_idx = None;
    for seq in items.iter() {
        let mut seq_max_idx = None;
        for (idx, &comp) in shortest.iter().enumerate() {
            if seq.get(idx) != Some(&comp) {
                break;
            }
            seq_max_idx = Some(idx);
        }
        if max_idx.is_none() || seq_max_idx < max_idx {
            max_idx = seq_max_idx;
        }
    }

    if let Some(max_idx) = max_idx {
        Some(&shortest[..=max_idx])
    } else {
        None
    }
}

pub fn find_common_prefix<'a, I: Iterator<Item = &'a str>>(iter: I) -> Option<String> {
    let mut items: Vec<Cow<'_, [&str]>> = iter
        .filter(|x| is_abs_path(x))
        .map(|x| Cow::Owned(split_path(x)))
        .collect();
    items.sort_by_key(|x| x.len());

    if let Some(slice) = find_common_prefix_of_sorted_vec(&items) {
        let rv = slice.join("");
        if !rv.is_empty() && &rv != "/" {
            return Some(rv);
        }
    }

    None
}

/// Helper function to calculate the path from a base file to a target file.
///
/// This is intended to caculate the path from a minified JavaScript file
/// to a sourcemap if they are both on the same server.
///
/// Example:
///
/// ```
/// # use swc_sourcemap::make_relative_path;
/// assert_eq!(&make_relative_path(
///     "/foo/bar/baz.js", "/foo/baz.map"), "../baz.map");
/// ```
pub fn make_relative_path(base: &str, target: &str) -> String {
    let target_path: Vec<_> = target
        .split(&['/', '\\'][..])
        .filter(|x| !x.is_empty())
        .collect();
    let mut base_path: Vec<_> = base
        .split(&['/', '\\'][..])
        .filter(|x| !x.is_empty())
        .collect();
    base_path.pop();

    let mut items = vec![
        Cow::Borrowed(target_path.as_slice()),
        Cow::Borrowed(base_path.as_slice()),
    ];
    items.sort_by_key(|x| x.len());

    let prefix = find_common_prefix_of_sorted_vec(&items)
        .map(|x| x.len())
        .unwrap_or(0);
    let mut rel_list: Vec<_> = iter::repeat_n("../", base_path.len() - prefix).collect();
    rel_list.extend_from_slice(&target_path[prefix..]);
    if rel_list.is_empty() {
        ".".into()
    } else {
        rel_list.join("")
    }
}

pub fn greatest_lower_bound<'a, T, K: Ord, F: Fn(&'a T) -> K>(
    slice: &'a [T],
    key: &K,
    map: F,
) -> Option<(usize, &'a T)> {
    let mut idx = match slice.binary_search_by_key(key, &map) {
        Ok(index) => index,
        Err(index) => {
            // If there is no match, then we know for certain that the index is where we should
            // insert a new token, and that the token directly before is the greatest lower bound.
            return slice.get(index.checked_sub(1)?).map(|res| (index, res));
        }
    };

    // If we get an exact match, then we need to continue looking at previous tokens to see if
    // they also match. We use a linear search because the number of exact matches is generally
    // very small, and almost certainly smaller than the number of tokens before the index.
    for i in (0..idx).rev() {
        if map(&slice[i]) == *key {
            idx = i;
        } else {
            break;
        }
    }
    slice.get(idx).map(|res| (idx, res))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_is_abs_path() {
        assert!(is_abs_path("C:\\foo.txt"));
        assert!(is_abs_path("d:/foo.txt"));
        assert!(!is_abs_path("foo.txt"));
        assert!(is_abs_path("/foo.txt"));
        assert!(is_abs_path("/"));
    }

    #[test]
    fn test_split_path() {
        assert_eq!(split_path("/foo/bar/baz"), &["", "/foo", "/bar", "/baz"]);
    }

    #[test]
    fn test_find_common_prefix() {
        let rv = find_common_prefix(vec!["/foo/bar/baz", "/foo/bar/baz/blah"].into_iter());
        assert_eq!(rv, Some("/foo/bar/baz".into()));

        let rv = find_common_prefix(vec!["/foo/bar/baz", "/foo/bar/baz/blah", "/meh"].into_iter());
        assert_eq!(rv, None);

        let rv = find_common_prefix(vec!["/foo/bar/baz", "/foo/bar/baz/blah", "/foo"].into_iter());
        assert_eq!(rv, Some("/foo".into()));

        let rv = find_common_prefix(vec!["/foo/bar/baz", "/foo/bar/baz/blah", "foo"].into_iter());
        assert_eq!(rv, Some("/foo/bar/baz".into()));

        let rv = find_common_prefix(
            vec!["/foo/bar/baz", "/foo/bar/baz/blah", "/blah", "foo"].into_iter(),
        );
        assert_eq!(rv, None);

        let rv = find_common_prefix(
            vec!["/foo/bar/baz", "/foo/bar/baz/blah", "/blah", "foo"].into_iter(),
        );
        assert_eq!(rv, None);
    }

    #[test]
    fn test_make_relative_path() {
        assert_eq!(
            &make_relative_path("/foo/bar/baz.js", "/foo/bar/baz.map"),
            "baz.map"
        );
        assert_eq!(
            &make_relative_path("/foo/bar/.", "/foo/bar/baz.map"),
            "baz.map"
        );
        assert_eq!(
            &make_relative_path("/foo/bar/baz.js", "/foo/baz.map"),
            "../baz.map"
        );
        assert_eq!(&make_relative_path("foo.txt", "foo.js"), "foo.js");
        assert_eq!(&make_relative_path("blah/foo.txt", "foo.js"), "../foo.js");
    }

    #[test]
    fn test_greatest_lower_bound() {
        let cmp = |&(i, _id)| i;

        let haystack = vec![(1, 1)];
        assert_eq!(greatest_lower_bound(&haystack, &1, cmp).unwrap().1, &(1, 1));
        assert_eq!(greatest_lower_bound(&haystack, &2, cmp).unwrap().1, &(1, 1));
        assert_eq!(greatest_lower_bound(&haystack, &0, cmp), None);

        let haystack = vec![(1, 1), (1, 2)];
        assert_eq!(greatest_lower_bound(&haystack, &1, cmp).unwrap().1, &(1, 1));
        assert_eq!(greatest_lower_bound(&haystack, &2, cmp).unwrap().1, &(1, 2));
        assert_eq!(greatest_lower_bound(&haystack, &0, cmp), None);

        let haystack = vec![(1, 1), (1, 2), (1, 3)];
        assert_eq!(greatest_lower_bound(&haystack, &1, cmp).unwrap().1, &(1, 1));
        assert_eq!(greatest_lower_bound(&haystack, &2, cmp).unwrap().1, &(1, 3));
        assert_eq!(greatest_lower_bound(&haystack, &0, cmp), None);

        let haystack = vec![(1, 1), (1, 2), (1, 3), (1, 4)];
        assert_eq!(greatest_lower_bound(&haystack, &1, cmp).unwrap().1, &(1, 1));
        assert_eq!(greatest_lower_bound(&haystack, &2, cmp).unwrap().1, &(1, 4));
        assert_eq!(greatest_lower_bound(&haystack, &0, cmp), None);

        let haystack = vec![(1, 1), (1, 2), (1, 3), (1, 4), (1, 5)];
        assert_eq!(greatest_lower_bound(&haystack, &1, cmp).unwrap().1, &(1, 1));
        assert_eq!(greatest_lower_bound(&haystack, &2, cmp).unwrap().1, &(1, 5));
        assert_eq!(greatest_lower_bound(&haystack, &0, cmp), None);
    }
}
