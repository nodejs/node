use std::io::{BufRead, BufReader, Read};
use std::str;

use crate::decoder::{decode_data_url, strip_junk_header, StripHeaderReader};
use crate::errors::Result;
use crate::jsontypes::MinimalRawSourceMap;
use crate::types::DecodedMap;

use url::Url;

/// Represents a reference to a sourcemap
#[derive(PartialEq, Eq, Debug)]
pub enum SourceMapRef {
    /// A regular URL reference
    Ref(String),
    /// A legacy URL reference
    LegacyRef(String),
}

fn resolve_url(ref_url: &str, minified_url: &Url) -> Option<Url> {
    minified_url.join(ref_url).ok()
}

impl SourceMapRef {
    /// Return the URL of the reference
    pub fn get_url(&self) -> &str {
        match *self {
            SourceMapRef::Ref(ref u) => u.as_str(),
            SourceMapRef::LegacyRef(ref u) => u.as_str(),
        }
    }

    /// Resolves the reference.
    ///
    /// The given minified URL needs to be the URL of the minified file.  The
    /// result is the fully resolved URL of where the source map can be located.
    pub fn resolve(&self, minified_url: &str) -> Option<String> {
        let url = self.get_url();
        if url.starts_with("data:") {
            return None;
        }
        resolve_url(url, &Url::parse(minified_url).ok()?).map(|x| x.to_string())
    }

    /// Resolves the reference against a local file path
    ///
    /// This is similar to `resolve` but operates on file paths.
    #[cfg(any(unix, windows, target_os = "redox"))]
    pub fn resolve_path(&self, minified_path: &std::path::Path) -> Option<std::path::PathBuf> {
        let url = self.get_url();
        if url.starts_with("data:") {
            return None;
        }
        resolve_url(url, &Url::from_file_path(minified_path).ok()?)
            .and_then(|x| x.to_file_path().ok())
    }

    /// Load an embedded sourcemap if there is a data URL.
    pub fn get_embedded_sourcemap(&self) -> Result<Option<DecodedMap>> {
        let url = self.get_url();
        if url.starts_with("data:") {
            Ok(Some(decode_data_url(url)?))
        } else {
            Ok(None)
        }
    }
}

/// Locates a sourcemap reference
///
/// Given a reader to a JavaScript file this tries to find the correct
/// sourcemap reference comment and return it.
pub fn locate_sourcemap_reference<R: Read>(rdr: R) -> Result<Option<SourceMapRef>> {
    for line in BufReader::new(rdr).lines() {
        let line = line?;
        if line.starts_with("//# sourceMappingURL=") || line.starts_with("//@ sourceMappingURL=") {
            let url = str::from_utf8(&line.as_bytes()[21..])?.trim().to_owned();
            if line.starts_with("//@") {
                return Ok(Some(SourceMapRef::LegacyRef(url)));
            } else {
                return Ok(Some(SourceMapRef::Ref(url)));
            }
        }
    }
    Ok(None)
}

/// Locates a sourcemap reference in a slice
///
/// This is an alternative to `locate_sourcemap_reference` that operates
/// on slices.
pub fn locate_sourcemap_reference_slice(slice: &[u8]) -> Result<Option<SourceMapRef>> {
    locate_sourcemap_reference(slice)
}

fn is_sourcemap_common(rsm: MinimalRawSourceMap) -> bool {
    (rsm.version.is_some() || rsm.file.is_some())
        && ((rsm.sources.is_some()
            || rsm.source_root.is_some()
            || rsm.sources_content.is_some()
            || rsm.names.is_some())
            && rsm.mappings.is_some())
        || rsm.sections.is_some()
}

fn is_sourcemap_impl<R: Read>(rdr: R) -> Result<bool> {
    let mut rdr = StripHeaderReader::new(rdr);
    let mut rdr = BufReader::new(&mut rdr);
    let rsm: MinimalRawSourceMap = serde_json::from_reader(&mut rdr)?;
    Ok(is_sourcemap_common(rsm))
}

fn is_sourcemap_slice_impl(slice: &[u8]) -> Result<bool> {
    let content = strip_junk_header(slice)?;
    let rsm: MinimalRawSourceMap = serde_json::from_slice(content)?;
    Ok(is_sourcemap_common(rsm))
}

/// Checks if a valid sourcemap can be read from the given reader
pub fn is_sourcemap<R: Read>(rdr: R) -> bool {
    is_sourcemap_impl(rdr).unwrap_or(false)
}

/// Checks if the given byte slice contains a sourcemap
pub fn is_sourcemap_slice(slice: &[u8]) -> bool {
    is_sourcemap_slice_impl(slice).unwrap_or(false)
}
