use anyhow::{bail, Context, Result};
use bytes_str::BytesStr;
use serde::{Deserialize, Serialize};
use swc_sourcemap::{vlq::parse_vlq_segment, RawToken, SourceMap};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum SourceMapContent {
    Json(String),
    #[serde(rename_all = "camelCase")]
    Parsed {
        #[serde(default)]
        sources: Vec<BytesStr>,
        #[serde(default)]
        names: Vec<BytesStr>,
        #[serde(default)]
        mappings: String,
        #[serde(default)]
        range_mappings: String,
        #[serde(default)]
        file: Option<BytesStr>,
        #[serde(default)]
        source_root: Option<String>,
        #[serde(default)]
        sources_content: Option<Vec<Option<BytesStr>>>,
    },
}

impl SourceMapContent {
    pub fn to_sourcemap(&self) -> Result<SourceMap> {
        match self {
            SourceMapContent::Json(s) => {
                SourceMap::from_slice(s.as_bytes()).context("failed to parse sourcemap")
            }
            SourceMapContent::Parsed {
                sources,
                names,
                mappings,
                range_mappings: _,
                file,
                source_root,
                sources_content,
            } => {
                let mut dst_col;
                let mut src_id = 0;
                let mut src_line = 0;
                let mut src_col = 0;
                let mut name_id = 0;

                let allocation_size = mappings.matches(&[',', ';'][..]).count() + 10;
                let mut tokens = Vec::with_capacity(allocation_size);

                let mut nums = Vec::with_capacity(6);

                for (dst_line, line) in mappings.split(';').enumerate() {
                    if line.is_empty() {
                        continue;
                    }

                    dst_col = 0;

                    for segment in line.split(',') {
                        if segment.is_empty() {
                            continue;
                        }

                        nums.clear();
                        nums = parse_vlq_segment(segment)?;
                        dst_col = (i64::from(dst_col) + nums[0]) as u32;

                        let mut src = !0;
                        let mut name = !0;

                        if nums.len() > 1 {
                            if nums.len() != 4 && nums.len() != 5 {
                                bail!(
                                    "invalid vlq segment size; expected 4 or 5, got {}",
                                    nums.len()
                                );
                            }
                            src_id = (i64::from(src_id) + nums[1]) as u32;
                            if src_id >= sources.len() as u32 {
                                bail!("invalid source reference: {}", src_id);
                            }

                            src = src_id;
                            src_line = (i64::from(src_line) + nums[2]) as u32;
                            src_col = (i64::from(src_col) + nums[3]) as u32;

                            if nums.len() > 4 {
                                name_id = (i64::from(name_id) + nums[4]) as u32;
                                if name_id >= names.len() as u32 {
                                    bail!("invalid name reference: {}", name_id);
                                }
                                name = name_id;
                            }
                        }

                        tokens.push(RawToken {
                            dst_line: dst_line as u32,
                            dst_col,
                            src_line,
                            src_col,
                            src_id: src,
                            name_id: name,
                            is_range: false,
                        });
                    }
                }

                let mut map = SourceMap::new(
                    file.clone(),
                    tokens,
                    names.clone(),
                    sources.clone(),
                    sources_content.clone(),
                );
                map.set_source_root(source_root.clone());
                Ok(map)
            }
        }
    }
}
