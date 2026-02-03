// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::{
    cell::RefCell,
    cmp::Ordering,
    collections::{BTreeMap, HashMap, HashSet},
    fmt,
    rc::Rc,
};

use crate::bundle::{Key, Resource, ResourceBundle};

use super::{
    header::{BinHeader, BinReprInfo},
    BinIndex, CharsetFamily, Endianness, FormatVersion, ResDescriptor, ResourceReprType,
};

const DATA_FORMAT: &[u8; 4] = b"ResB";
const DATA_VERSION: &[u8; 4] = &[1, 4, 0, 0];
const PADDED_HEADER_SIZE: usize = (core::mem::size_of::<BinHeader>() + 15) & !0xf;
const REPR_INFO_SIZE: usize = core::mem::size_of::<BinReprInfo>();

/// The value of the magic word appearing in the header.
///
/// All versions as of [`FormatVersion::V3_0`] share the same value.
const MAGIC_WORD: &[u8; 2] = &[0xda, 0x27];

/// The size of the characters used in representations of string resources.
const SIZE_OF_STRING_CHAR: u8 = 2;

/// The endianness of the current system.
#[cfg(target_endian = "little")]
const SYSTEM_ENDIANNESS: Endianness = Endianness::Little;

/// The endianness of the current system.
#[cfg(target_endian = "big")]
const SYSTEM_ENDIANNESS: Endianness = Endianness::Big;

/// The `StringResourceData` struct encapsulates information necessary for
/// building string resource representations.
#[derive(Debug)]
struct StringResourceData<'a> {
    /// If this string is a suffix of another string, the full string which
    /// contains it. See [`build_string_data`] for more details.
    ///
    /// [`build_string_data`]: Writer::build_string_data
    containing_string: Option<&'a str>,

    /// The offset of the string within the 16-bit data block.
    offset: u32,

    /// The number of copies of this string which appear in the bundle.
    copy_count: usize,

    /// The number of characters saved by deduplicating and suffix-sharing this
    /// string.
    characters_saved: usize,
}

/// Data necessary for building resource representations by resource type.
#[derive(Debug)]
enum BinResourceTypeData<'a> {
    String {
        /// The string represented by the resource.
        string: &'a str,

        /// A reference to string resource data which can be shared among string
        /// resources with the same represented string.
        data: Rc<RefCell<StringResourceData<'a>>>,
    },
    Array {
        /// Resource data for all children of the array.
        children: Vec<BinResourceData<'a>>,
    },
    Table {
        /// Resource data for all entries of the table.
        map: BTreeMap<Key<'a>, BinResourceData<'a>>,
    },
    Binary {
        /// The binary data represented by the resource.
        binary: &'a [u8],
    },
    Integer,
    IntVector {
        /// The integers contained in the vector.
        int_vector: &'a [u32],
    },
    _Alias,
}

impl std::fmt::Display for BinResourceTypeData<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // Provide a user-friendly string for the resource type in order to
        // create helpful error messages.
        write!(
            f,
            "{}",
            match self {
                BinResourceTypeData::String { .. } => "String",
                BinResourceTypeData::Array { .. } => "Array",
                BinResourceTypeData::Table { .. } => "Table",
                BinResourceTypeData::Binary { .. } => "Binary",
                BinResourceTypeData::Integer => "Integer",
                BinResourceTypeData::IntVector { .. } => "IntVector",
                BinResourceTypeData::_Alias => "Alias",
            }
        )
    }
}

/// The `BinResourceData` struct maintains data necessary for representing a
/// resource as part of a binary resource bundle.
#[derive(Debug)]
struct BinResourceData<'a> {
    /// The type descriptor for this resource if it has been processed.
    descriptor: Option<ResDescriptor>,

    /// Any per-type data necessary for representing this resource.
    type_data: BinResourceTypeData<'a>,
}

impl<'a> BinResourceData<'a> {
    /// Prepares a new data struct for processing a resource.
    fn new(type_data: BinResourceTypeData<'a>) -> Self {
        Self {
            descriptor: None,
            type_data,
        }
    }
}

impl<'a> From<&'a Resource<'a>> for BinResourceData<'a> {
    fn from(value: &'a Resource<'a>) -> Self {
        match value {
            Resource::String(string) => Self::new(BinResourceTypeData::String {
                string,
                data: Rc::new(RefCell::new(StringResourceData {
                    containing_string: None,
                    offset: 0,
                    copy_count: 0,
                    characters_saved: 0,
                })),
            }),
            Resource::Array(array) => {
                // Build resource data structs for each child in the array.
                let mut children = Vec::new();
                for resource in array {
                    children.push(Self::from(resource));
                }

                Self::new(BinResourceTypeData::Array { children })
            }
            Resource::Table(table) => {
                // Build resource data structs for each entry in the table.
                let map = table
                    .iter()
                    .map(|(key, resource)| (key.clone(), BinResourceData::from(resource)))
                    .collect::<BTreeMap<_, _>>();

                Self::new(BinResourceTypeData::Table { map })
            }
            Resource::Binary(binary) => Self::new(BinResourceTypeData::Binary { binary }),
            Resource::Integer(integer) => Self {
                // Integers are stored as part of the resource descriptor, so no
                // further processing is needed.
                descriptor: Some(ResDescriptor::new(ResourceReprType::Int, (*integer).into())),
                type_data: BinResourceTypeData::Integer,
            },
            Resource::IntVector(int_vector) => {
                Self::new(BinResourceTypeData::IntVector { int_vector })
            }
        }
    }
}

/// The `Serializer` type provides a means of generating a vector of bytes
/// representing a [`ResourceBundle`] in the ICU binary resource bundle
/// format.
#[derive(Debug)]
pub struct Serializer {
    /// The format version for which to generate the binary bundle.
    format_version: FormatVersion,

    /// The count of entries in the largest table in the bundle.
    ///
    /// Used for generating the [`BinIndex`].
    largest_table_entry_count: u32,
}

impl Serializer {
    /// Makes a new write state.
    fn new(format_version: FormatVersion) -> Self {
        Self {
            format_version,
            largest_table_entry_count: 0,
        }
    }

    /// Generates a representation of a [`ResourceBundle`] in the binary
    /// resource bundle format.
    ///
    /// Returns a vector of bytes containing the binary representation on
    /// success.
    pub fn to_bytes(
        bundle: &ResourceBundle,
        keys_in_discovery_order: &[Key],
    ) -> Result<Vec<u8>, BinarySerializerError> {
        // For now, we hardcode the format version value and do not support
        // writing pool bundles.
        let format_version = FormatVersion::V2_0;
        let write_pool_bundle_checksum = false;

        let mut serializer = Self::new(format_version);

        // Walk the resource tree to generate the necessary structures for
        // tracking write state for each resource.
        let mut root = BinResourceData::from(bundle.root());

        // Determine the size of the index. We can't generate the index until
        // we've finished building the rest of the file, but its size is
        // important for creating resource offsets.
        let index_field_count = match format_version {
            // Format version 1.0 does not include an index.
            FormatVersion::V1_0 => 0,
            // Format version 1.1 specifies the index, with fields `field_count`
            // through `largest_table_entry_count`.
            FormatVersion::V1_1 => 5,
            // Format version 1.2 adds the `bundle_attributes` field.
            FormatVersion::V1_2 | FormatVersion::V1_3 => 6,
            // Format version 2.0 adds the `data_16_bit_end` field and, if the
            // bundle is either a pool bundle or uses a pool bundle, the
            // `pool_checksum` field.
            FormatVersion::V2_0 | FormatVersion::V3_0 => {
                if write_pool_bundle_checksum {
                    8
                } else {
                    7
                }
            }
        };

        // The root descriptor is considered to be at offset `0`, so include it
        // in the determination of the offset of the end of the index.
        let index_end = (1 + index_field_count) * std::mem::size_of::<u32>() as u32;

        // Build the key block.
        let mut key_position_map = HashMap::new();
        let keys = serializer.build_key_block(
            &root,
            index_end,
            keys_in_discovery_order,
            &mut key_position_map,
        );
        let keys_end = index_end + keys.len() as u32;

        // Build the 16-bit data block.
        let data_16_bit = serializer.build_16_bit_data_block(&mut root, &key_position_map)?;
        let data_16_bit_end = keys_end + data_16_bit.len() as u32;

        // Build the resource block.
        let resources =
            serializer.build_resource_block(&mut root, data_16_bit_end, &key_position_map)?;
        let resources_end = data_16_bit_end + resources.len() as u32;

        // Build the index now that we know the sizes of all blocks.
        let index = serializer.build_index(
            bundle,
            index_field_count,
            keys_end >> 2,
            resources_end >> 2,
            data_16_bit_end >> 2,
        );

        // Build the final structs for writing.
        let repr_info = BinReprInfo {
            size: REPR_INFO_SIZE as u16,
            reserved_word: 0,
            endianness: SYSTEM_ENDIANNESS,
            charset_family: CharsetFamily::Ascii,
            size_of_char: SIZE_OF_STRING_CHAR,
            reserved_byte: 0,
            data_format: *DATA_FORMAT,
            format_version: serializer.format_version,
            data_version: *DATA_VERSION,
        };

        let header = BinHeader {
            size: PADDED_HEADER_SIZE as u16,
            magic: *MAGIC_WORD,
            repr_info,
        };

        let root_descriptor = root.descriptor.ok_or(BinarySerializerError::unexpected(
            "root descriptor was never populated",
        ))?;
        let bundle_struct = BinResBundle {
            header,
            root_descriptor,
            index,
            keys: &keys,
            data_16_bit: &data_16_bit,
            resources: &resources,
        };

        // Write the bundle as bytes and return.
        Vec::<u8>::try_from(bundle_struct)
    }

    /// Collects the set of keys used in tables.
    fn collect_keys<'a>(resource: &BinResourceTypeData<'a>, collected_keys: &mut HashSet<Key<'a>>) {
        match resource {
            BinResourceTypeData::Table { map } => {
                for (key, resource) in map {
                    collected_keys.insert(key.clone());
                    Self::collect_keys(&resource.type_data, collected_keys);
                }
            }
            BinResourceTypeData::Array { children } => {
                for child in children {
                    Self::collect_keys(&child.type_data, collected_keys);
                }
            }
            _ => (),
        }
    }

    /// Generates a vector of bytes representing the block of table keys.
    ///
    /// Returns the vector of bytes.
    fn build_key_block<'a>(
        &self,
        root: &BinResourceData,
        block_start_position: u32,
        keys_in_discovery_order: &[Key<'a>],
        key_position_map: &mut HashMap<Key<'a>, u32>,
    ) -> Vec<u8> {
        let mut encountered_keys = HashSet::new();
        Self::collect_keys(&root.type_data, &mut encountered_keys);

        // Get the keys sorted in the order we encountered them during parse.
        // While not strictly a part of the specification, ICU4C's `genrb` tool
        // writes keys into the binary bundle in this order and so that behavior
        // is replicated here.
        let sorted_keys = keys_in_discovery_order
            .iter()
            .filter(|&key| encountered_keys.contains(key));

        let mut key_block = Vec::new();
        for key in sorted_keys {
            // Track the position of each key as we insert it for later
            // reference in writing tables.
            key_position_map.insert(key.clone(), block_start_position + key_block.len() as u32);

            // Write keys as null-terminated 8-bit strings.
            key_block.append(&mut key.as_bytes().to_vec());
            key_block.push(0);
        }

        // Pad the key block such that the end is aligned with a 32-bit
        // boundary.
        let padding = (block_start_position as usize + key_block.len()) % 3;
        key_block.resize(key_block.len() + padding, 0xaa);

        key_block
    }

    /// Collects the set of strings appearing in string resources and tracks the
    /// number of copies of each.
    fn collect_strings<'a>(
        resource: &mut BinResourceData<'a>,
        strings: &mut HashMap<&'a str, Rc<RefCell<StringResourceData<'a>>>>,
    ) {
        let mut existing_string_data = None;
        match &mut resource.type_data {
            BinResourceTypeData::String { string, data } => {
                if strings.contains_key(string) {
                    // Record the data struct for the already-encountered copy
                    // of this string so that we can reuse it for this copy. We
                    // want each instance of a string to share a reference to
                    // the same data in order to allow multiple string resources
                    // to share a single set of bytes in the 16-bit data block.
                    // We can't use `if let` here because we need to borrow as
                    // mutable in the `else` case.
                    #[expect(clippy::unwrap_used)]
                    let data = strings.get(string).unwrap();
                    data.borrow_mut().copy_count += 1;
                    existing_string_data = Some((string, data));
                } else {
                    strings.insert(string, data.clone());
                    data.borrow_mut().copy_count += 1;
                }
            }

            // Walk the resource tree.
            BinResourceTypeData::Array { children } => {
                for child in children {
                    Self::collect_strings(child, strings);
                }
            }
            BinResourceTypeData::Table { map, .. } => {
                for resource in map.values_mut() {
                    Self::collect_strings(resource, strings);
                }
            }
            _ => (),
        };

        // We will only have recorded string data for this resource if it is a
        // string resource.
        if let Some((string, data)) = existing_string_data {
            // Ensure that this string resource uses the same data as every
            // string resource with a matching string.
            resource.type_data = BinResourceTypeData::String {
                string,
                data: data.clone(),
            }
        };
    }

    /// Generates a vector of 16-bit values representing the UTF-16 string
    /// resources present in the bundle.
    ///
    /// This builds a block containing all of the string resources in the
    /// bundle, which are compacted by deduplicating strings and reusing the
    /// ends of longer strings when they wholly contain a shorter string
    /// ("suffix").
    fn build_string_data(
        &mut self,
        root: &mut BinResourceData,
        data_16_bit: &mut Vec<u16>,
    ) -> Result<(), BinarySerializerError> {
        let mut strings = HashMap::new();
        Self::collect_strings(root, &mut strings);
        let count = strings.len();

        // Sort the strings such that any suffixes occur immediately after their
        // containing string.
        let mut sorted_strings = strings.keys().cloned().collect::<Vec<_>>();
        sorted_strings.sort_unstable_by(cmp_string_descending_suffix_aware);

        // Locate strings which are suffixes of other strings and link them to
        // their containing string.
        for (i, string) in sorted_strings.iter().enumerate() {
            // We can safely unwrap here as `sorted_strings` is just a sorted
            // list of keys for the map in question.
            #[expect(clippy::unwrap_used)]
            let data = strings.get(string).unwrap();

            if data.borrow().containing_string.is_some() {
                // We've already processed this string as a suffix in the inner
                // loop.
                continue;
            }

            // Calculate the total number of characters saved by deduplicating
            // copies of this string.
            let copy_count = data.borrow().copy_count;
            data.borrow_mut().characters_saved = (copy_count - 1) * get_total_string_size(string)?;

            for suffix in sorted_strings.iter().take(count).skip(i + 1) {
                if !string.ends_with(suffix) {
                    // This string is not a suffix of the preceding string;
                    // because suffixes are sorted immediately after their
                    // containing string, no further strings will be
                    break;
                }

                if get_string_length_marker_size(suffix)? != 0 {
                    // Skip the suffix if it is long enough to require an
                    // explicit length marker, as we can't include those in the
                    // middle of another string.
                    continue;
                }

                // Note the offset of the suffix into its containing string and
                // link the two. We can safely unwrap here as `sorted_strings`
                // are all keys for the map in question.
                #[expect(clippy::unwrap_used)]
                let suffix_data = strings.get(suffix).unwrap();
                suffix_data.borrow_mut().offset =
                    (string.chars().count() - suffix.chars().count()) as u32;
                suffix_data.borrow_mut().containing_string = Some(string);

                // Update the characters saved by the containing string.
                data.borrow_mut().characters_saved +=
                    suffix_data.borrow().copy_count * get_total_string_size(suffix)?;
            }
        }

        // Sort the strings such that suffixes are in ascending length order
        // with suffixes sorted to the end. Additionally, strings which save
        // more characters are sorted earlier in order to maximize space savings
        // in the pool bundle case.
        //
        // Ascending length order allows for the maximum number of strings to be
        // addressable by 16-bit offsets in cases where there are a large number
        // of strings.
        let mut sorted_strings = strings
            .iter()
            .collect::<Vec<(&&str, &Rc<RefCell<StringResourceData>>)>>();
        sorted_strings.sort_unstable_by(cmp_string_ascending_suffix_aware);

        for (string, data) in sorted_strings {
            if data.borrow().containing_string.is_some() {
                // This string is a suffix of another. Because suffixes are
                // sorted to the end, we are guaranteed to have already written
                // the containing string. We can't use `if let` here because we
                // need to borrow as mutable, but we can safely unwrap.
                #[expect(clippy::unwrap_used)]
                let containing_string = data.borrow().containing_string.unwrap();
                let containing_data =
                    strings
                        .get(containing_string)
                        .ok_or(BinarySerializerError::unexpected(
                            "containing string not present in string map",
                        ))?;

                // Update the offset of the suffix from a relative position in
                // the containing string to an absolute position in the 16-bit
                // data block. Ensure that we account for the containing
                // string's length marker.
                let containing_offset = containing_data.borrow().offset
                    + get_string_length_marker_size(containing_string)? as u32;
                data.borrow_mut().offset += containing_offset;

                continue;
            }

            data.borrow_mut().offset = data_16_bit.len() as u32;

            // Build a length marker for the string if one is required.
            let length = string.chars().count();
            let length_words = get_string_length_marker_size(string)?;

            match length_words {
                0 => (),
                1 => data_16_bit.push(0xdc00 & length as u16),
                2 => {
                    data_16_bit.push(0xdfef + (length >> 16) as u16);
                    data_16_bit.push(length as u16);
                }
                3 => {
                    data_16_bit.push(0xdfff);
                    data_16_bit.push((length >> 16) as u16);
                    data_16_bit.push(length as u16);
                }
                _ => return Err(BinarySerializerError::string_too_long(length)),
            };

            // Write the string to the 16-bit data block as UTF-16.
            data_16_bit.append(&mut string.encode_utf16().collect());
            data_16_bit.push(0);
        }

        Ok(())
    }

    /// Generates a vector of 16-bit values representing the string block and
    /// any collections whose contents can be addressed by 16-bit values.
    ///
    /// Returns a 16-bit offset to the resource from the start of the 16-bit
    /// data block if the resource can be stored in the block.
    fn build_16_bit_resource_data(
        &mut self,
        resource: &mut BinResourceData,
        data_16_bit: &mut Vec<u16>,
        key_position_map: &HashMap<Key, u32>,
    ) -> Option<u16> {
        match &mut resource.type_data {
            BinResourceTypeData::String { data, .. } => {
                let string_offset = data.borrow().offset;

                // We've already processed the string resources into 16-bit
                // data. Add resource descriptors to allow 32-bit collections to
                // address them.
                resource.descriptor = Some(ResDescriptor::new(
                    ResourceReprType::StringV2,
                    string_offset,
                ));

                // If this string is addressable by a 16-bit offset, return
                // that offset.
                match string_offset <= u16::MAX as u32 {
                    true => Some(string_offset as u16),
                    false => None,
                }
            }
            BinResourceTypeData::Array { ref mut children } => {
                if children.is_empty() {
                    // We match ICU4C's `genrb` tool in representing empty
                    // arrays as the `Array` type.
                    return None;
                }

                let mut data_16_bit_offsets = Vec::new();
                for child in &mut *children {
                    let offset =
                        self.build_16_bit_resource_data(child, data_16_bit, key_position_map);

                    // If the offset is `None`, it isn't possible to represent
                    // this array as an `Array16`, but we continue to walk the
                    // tree in case any of its children are representable in 16
                    // bits.
                    data_16_bit_offsets.push(offset);
                }

                // If any of this array's children can't be represented as a
                // 16-bit resource, it cannot be represented as an `Array16`.
                let data_16_bit_offsets = data_16_bit_offsets
                    .into_iter()
                    .collect::<Option<Vec<_>>>()?;

                // Write the array as an `Array16` and update the resource data
                // appropriately.
                resource.descriptor = Some(ResDescriptor::new(
                    ResourceReprType::Array16,
                    data_16_bit.len() as u32,
                ));

                data_16_bit.push(children.len() as u16);
                for offset in data_16_bit_offsets {
                    data_16_bit.push(offset);
                }

                None
            }
            BinResourceTypeData::Table { ref mut map, .. } => {
                if map.is_empty() {
                    // We match ICU4C's `genrb` tool in representing empty
                    // tables as the `Table` type.
                    return None;
                }

                let mut data_16_bit_offsets = Vec::new();
                for resource in map.values_mut() {
                    let offset =
                        self.build_16_bit_resource_data(resource, data_16_bit, key_position_map);

                    // If the offset is `None`, it isn't possible to represent
                    // this table as a `Table16`, but we continue to walk the
                    // tree in case any of its children are representable in 16
                    // bits.
                    data_16_bit_offsets.push(offset);
                }

                // If any of this table's children can't be represented as a
                // 16-bit resource, it cannot be represented as a `Table16`.
                let data_16_bit_offsets = data_16_bit_offsets
                    .into_iter()
                    .collect::<Option<Vec<_>>>()?;

                // Update the largest table value as appropriate.
                let size = map.len() as u32;
                self.largest_table_entry_count =
                    std::cmp::max(self.largest_table_entry_count, size);

                // Write the table as a `Table16` and update the resource data
                // appropriately.
                resource.descriptor = Some(ResDescriptor::new(
                    ResourceReprType::Table16,
                    data_16_bit.len() as u32,
                ));

                data_16_bit.push(size as u16);
                for position in key_position_map.values() {
                    data_16_bit.push(*position as u16);
                }
                for descriptor in data_16_bit_offsets {
                    data_16_bit.push(descriptor);
                }

                None
            }
            _ => None,
        }
    }

    /// Generates a vector of bytes representing the 16-bit resource block.
    ///
    /// Returns the vector of bytes on success.
    fn build_16_bit_data_block(
        &mut self,
        root: &mut BinResourceData,
        key_position_map: &HashMap<Key, u32>,
    ) -> Result<Vec<u8>, BinarySerializerError> {
        // Begin the 16-bit data block with a 16-bit `0`. While ICU4C's `genrb`
        // tool does this in order to provide empty 16-bit collections with an
        // appropriate offset, the tool does not actually generate any such
        // collections. We retain this behavior solely for compatibility.
        let mut data_16_bit = vec![0];

        self.build_string_data(root, &mut data_16_bit)?;

        self.build_16_bit_resource_data(root, &mut data_16_bit, key_position_map);

        // Pad the 16-bit data block so that the end aligns with a 32-bit
        // boundary.
        if data_16_bit.len() & 1 != 0 {
            data_16_bit.push(0xaaaa);
        }

        // Reinterpret the 16-bit block as native-endian bytes to ease later
        // writing.
        let data_16_bit = data_16_bit
            .iter()
            .flat_map(|value| value.to_ne_bytes())
            .collect();

        Ok(data_16_bit)
    }

    /// Generates a vector of bytes representing a single 32-bit resource.
    ///
    /// Returns the resource descriptor for the resource on success..
    fn build_32_bit_resource(
        &mut self,
        resource: &mut BinResourceData,
        block_start_position: u32,
        data: &mut Vec<u8>,
        key_position_map: &HashMap<Key, u32>,
    ) -> Result<ResDescriptor, BinarySerializerError> {
        if let Some(descriptor) = resource.descriptor {
            // We've already processed this resource in an earlier step.
            return Ok(descriptor);
        }

        match &mut resource.type_data {
            BinResourceTypeData::Array { children } => {
                if children.is_empty() {
                    return Ok(ResDescriptor::_new_empty(ResourceReprType::Array));
                }

                // Add all child resources to the data and collect their
                // resource descriptors.
                let child_descriptors = children
                    .iter_mut()
                    .map(|child| {
                        self.build_32_bit_resource(
                            child,
                            block_start_position,
                            data,
                            key_position_map,
                        )
                    })
                    .collect::<Result<Vec<_>, BinarySerializerError>>()?;

                // Build a resource descriptor for this array.
                let offset = block_start_position + data.len() as u32;
                resource.descriptor =
                    Some(ResDescriptor::new(ResourceReprType::Array, offset >> 2));

                // Build the array representation.
                let mut vector = (children.len() as u32).to_ne_bytes().to_vec();
                data.append(&mut vector);

                for descriptor in child_descriptors {
                    data.append(&mut u32::from(descriptor).to_ne_bytes().to_vec());
                }
            }
            BinResourceTypeData::Table { map, .. } => {
                if map.is_empty() {
                    return Ok(ResDescriptor::_new_empty(ResourceReprType::Table));
                }

                // Update the largest table value as appropriate.
                let size = map.len() as u32;
                self.largest_table_entry_count =
                    std::cmp::max(self.largest_table_entry_count, size);

                // Add all child resources to the data and collect their
                // resource descriptors.
                let child_descriptors = map
                    .values_mut()
                    .map(|child| {
                        self.build_32_bit_resource(
                            child,
                            block_start_position,
                            data,
                            key_position_map,
                        )
                    })
                    .collect::<Result<Vec<_>, BinarySerializerError>>()?;

                // Build a resource descriptor for this table.
                let offset = block_start_position + data.len() as u32;
                resource.descriptor =
                    Some(ResDescriptor::new(ResourceReprType::Table, offset >> 2));

                // Build the table representation.
                data.append(&mut (map.len() as u16).to_ne_bytes().to_vec());

                for key in map.keys() {
                    let position =
                        key_position_map
                            .get(key)
                            .ok_or(BinarySerializerError::unexpected(
                                "key not present in position map",
                            ))?;
                    data.append(&mut (*position as u16).to_ne_bytes().to_vec());
                }

                // Pad the key listing to end at a 32-bit boundary.
                if map.len() & 1 == 0 {
                    data.resize(data.len() + 2, 0xaa);
                }

                for descriptor in child_descriptors {
                    data.append(&mut u32::from(descriptor).to_ne_bytes().to_vec());
                }
            }
            BinResourceTypeData::Binary { binary } => {
                if binary.is_empty() {
                    return Ok(ResDescriptor::_new_empty(ResourceReprType::Binary));
                }

                // Pad before the start of the binary data such that the number
                // of bytes in the body is divisible by 16.
                let offset = block_start_position as usize + data.len();
                let aligned = (offset + std::mem::size_of::<u32>()) % 16;
                if aligned != 0 {
                    data.resize(data.len() + (16 - aligned), 0xaa);
                }

                // Build a resource descriptor for the binary data.
                let offset = block_start_position + data.len() as u32;
                resource.descriptor =
                    Some(ResDescriptor::new(ResourceReprType::Binary, offset >> 2));

                // Build the binary data representation.
                data.append(&mut (binary.len() as u32).to_ne_bytes().to_vec());
                data.extend_from_slice(binary);
            }
            BinResourceTypeData::IntVector { int_vector } => {
                if int_vector.is_empty() {
                    return Ok(ResDescriptor::_new_empty(ResourceReprType::IntVector));
                }

                // Build a resource descriptor for the vector.
                let offset = block_start_position + data.len() as u32;
                resource.descriptor =
                    Some(ResDescriptor::new(ResourceReprType::IntVector, offset >> 2));

                // Build the vector representation.
                data.append(&mut (int_vector.len() as u32).to_ne_bytes().to_vec());

                for int in *int_vector {
                    data.append(&mut (*int).to_ne_bytes().to_vec());
                }
            }
            BinResourceTypeData::_Alias => todo!(),
            _ => {
                return Err(BinarySerializerError::unexpected(
                    "expected resource to have been processed already",
                ))
            }
        };

        // Pad the resource body to end at a 32-bit boundary.
        let position = block_start_position as usize + data.len();
        let u32_size = std::mem::size_of::<u32>();
        if position % u32_size != 0 {
            data.resize(data.len() + (u32_size - position % u32_size), 0xaa);
        }

        resource.descriptor.ok_or(BinarySerializerError::unexpected(
            "resource descriptor has not been populated",
        ))
    }

    /// Generates a vector of bytes representing the 32-bit resource block.
    ///
    /// Returns the vector of bytes on success.
    fn build_resource_block(
        &mut self,
        root: &mut BinResourceData,
        block_start_position: u32,
        key_position_map: &HashMap<Key, u32>,
    ) -> Result<Vec<u8>, BinarySerializerError> {
        let mut body = Vec::new();
        self.build_32_bit_resource(root, block_start_position, &mut body, key_position_map)?;

        Ok(body)
    }

    /// Generates the index block of the bundle data.
    ///
    /// Returns the final index as a struct.
    fn build_index(
        &self,
        bundle: &ResourceBundle,
        field_count: u32,
        keys_end: u32,
        resources_end: u32,
        data_16_bit_end: u32,
    ) -> BinIndex {
        BinIndex {
            field_count,
            keys_end,
            resources_end,
            bundle_end: resources_end,
            largest_table_entry_count: self.largest_table_entry_count,
            bundle_attributes: Some(!bundle.is_locale_fallback_enabled as u32),
            data_16_bit_end: Some(data_16_bit_end),
            pool_checksum: None,
        }
    }
}

/// Gets the total size of a UTF-16 string in 16-bit characters.
///
/// This count includes the string length marker and a null terminator.
fn get_total_string_size(string: &str) -> Result<usize, BinarySerializerError> {
    Ok(string.chars().count() + get_string_length_marker_size(string)? + 1)
}

/// Gets the size of the length marker of a UTF-16 string in 16-bit characters.
///
/// Strings of no more than 40 characters are terminated with a 16-bit U+0000
/// character, while longer strings are marked with one to three UTF-16 low
/// surrogates to indicate length.`
///
/// For more details, see [`ResourceReprType::StringV2`].
fn get_string_length_marker_size(string: &str) -> Result<usize, BinarySerializerError> {
    let length = match string.chars().count() {
        0..=40 => 0,
        41..=0x3ee => 1,
        0x3ef..=0xf_ffff => 2,
        0x10_0000..=0xffff_ffff => 3,
        length => return Err(BinarySerializerError::string_too_long(length)),
    };

    Ok(length)
}

/// Determines the suffix-aware descending length ordering of two keys.
///
/// When used to sort keys, any key which is wholly contained within the end of
/// another will be sorted immediately after it.
fn _cmp_key_suffix(l: &(usize, &str), r: &(usize, &str)) -> Ordering {
    let (l_pos, l_string) = l;
    let (r_pos, r_string) = r;
    let mut l_iter = l_string.chars();
    let mut r_iter = r_string.chars();

    // Iterate strings in reverse order so that, if one string runs out before
    // non-matching characters are found, it is a suffix of the other.
    while let Some((l_char, r_char)) = l_iter.next_back().zip(r_iter.next_back()) {
        match l_char.cmp(&r_char) {
            Ordering::Equal => (),
            ord => return ord,
        };
    }

    match r_string.chars().count().cmp(&l_string.chars().count()) {
        Ordering::Equal => l_pos.cmp(r_pos),
        ord => ord,
    }
}

/// Determines the suffix-aware ordering of two strings in descending order of
/// length.
///
/// When used to sort strings, any string which is wholly contained within the
/// end of another will be sorted immediately after it. See
/// [`build_string_data`] for more details.
///
/// [`build_string_data`]: Writer::build_string_data
fn cmp_string_descending_suffix_aware(l: &&str, r: &&str) -> Ordering {
    let mut l_iter = l.chars();
    let mut r_iter = r.chars();

    // Iterate strings in reverse order so that, if one string runs out before
    // non-matching characters are found, it is a suffix of the other.
    while let Some((l_char, r_char)) = l_iter.next_back().zip(r_iter.next_back()) {
        match l_char.cmp(&r_char) {
            Ordering::Equal => (),
            ord => return ord,
        }
    }

    // Sort matching strings in descending order of length, such that a suffix
    // sorts after its containing string.
    r.chars().count().cmp(&l.chars().count())
}

/// Determines the suffix-aware ordering of two strings in ascending order of
/// length.
///
/// When used to sort strings, any string which is a suffix of another will be
/// sorted to the end. See [`build_string_data`] for more details.
///
/// [`build_string_data`]: Writer::build_string_data
fn cmp_string_ascending_suffix_aware(
    l: &(&&str, &Rc<RefCell<StringResourceData>>),
    r: &(&&str, &Rc<RefCell<StringResourceData>>),
) -> Ordering {
    let (l, l_data) = l;
    let (r, r_data) = r;
    // If one string is a suffix and the other is not, the suffix should be
    // sorted after in order to guarantee that its containing string will be
    // processed first.
    match (
        l_data.borrow().containing_string,
        r_data.borrow().containing_string,
    ) {
        (Some(_), None) => return Ordering::Greater,
        (None, Some(_)) => return Ordering::Less,
        _ => (),
    };

    // Sort longer strings later.
    match l.chars().count().cmp(&r.chars().count()) {
        Ordering::Equal => (),
        ord => return ord,
    };

    // Strings with more duplicates or matching suffixes are sorted earlier.
    // This allows for maximizing savings in pool bundles. It is otherwise not
    // relevant.
    match r_data
        .borrow()
        .characters_saved
        .cmp(&l_data.borrow().characters_saved)
    {
        Ordering::Equal => (),
        ord => return ord,
    };

    // All other things being equal, resort to lexical sort.
    l.cmp(r)
}

/// The `BinResBundle` struct is designed to mimic the in-memory layout of a
/// binary resource bundle. It is used in constructing byte data for writing.
struct BinResBundle<'a> {
    /// The file header.
    ///
    /// As represented in the byte data, the header is zero-padded such that the
    /// total size of the header in bytes is divisible by 16.
    header: BinHeader,

    /// The resource descriptor for the root resource in the bundle. This is
    /// considered the `0`th 32-bit value in the body for purposes of
    /// resolving offsets.
    root_descriptor: ResDescriptor,

    /// Details of the final layout of the bundle as well as attributes for
    /// resolving resources.
    ///
    /// The index is present from [`FormatVersion::V1_1`] on.
    index: BinIndex,

    /// A block of strings representing keys in table resources.
    ///
    /// Keys are stored as contiguous null-terminated 8-bit strings in the
    /// core encoding of the [`CharsetFamily`] of the bundle.
    keys: &'a [u8],

    /// A block of 16-bit data containing 16-bit resources.
    ///
    /// This block is present from [`FormatVersion::V2_0`] on.
    ///
    /// 16-bit resources consist of UTF-16 string resources and collections
    /// which contain only 16-bit string resources. See
    /// [`ResourceReprType::StringV2`], [`ResourceReprType::Array16`], and
    /// [`ResourceReprType::Table16`] for details on representation.
    data_16_bit: &'a [u8],

    /// A block of resource representations. See [`ResourceReprType`] for
    /// details on the representation of resources.
    resources: &'a [u8],
}

impl TryFrom<BinResBundle<'_>> for Vec<u8> {
    type Error = BinarySerializerError;

    fn try_from(value: BinResBundle<'_>) -> Result<Self, Self::Error> {
        // Manually build the list of bytes to write to the output. There are
        // crates which provide this functionality, but the writing of the body
        // is sufficiently complex as to make the header the only part where
        // an external crate would be convenient, and that's a matter of a small
        // number of bytes, so on balance it's easier to just handle those here.
        let mut bytes = Vec::new();
        bytes.append(&mut Vec::<u8>::from(value.header));

        // Write the body.
        bytes.extend_from_slice(&u32::from(value.root_descriptor).to_ne_bytes());
        bytes.append(&mut Vec::<u8>::try_from(value.index)?);
        bytes.extend_from_slice(value.keys);
        bytes.extend_from_slice(value.data_16_bit);
        bytes.extend_from_slice(value.resources);

        Ok(bytes)
    }
}

impl From<BinHeader> for Vec<u8> {
    fn from(value: BinHeader) -> Self {
        let mut bytes = Vec::with_capacity(value.size as usize);

        bytes.extend_from_slice(&value.size.to_ne_bytes());
        bytes.extend_from_slice(&value.magic);

        let mut data_info_bytes = Vec::<u8>::from(value.repr_info);
        bytes.append(&mut data_info_bytes);

        // Pad the header such that its total size is divisible by 16.
        bytes.resize(PADDED_HEADER_SIZE, 0);

        bytes
    }
}

impl From<BinReprInfo> for Vec<u8> {
    fn from(value: BinReprInfo) -> Self {
        let mut bytes = Vec::with_capacity(value.size as usize);

        bytes.extend_from_slice(&value.size.to_ne_bytes());
        bytes.extend_from_slice(&value.reserved_word.to_ne_bytes());
        bytes.push(value.endianness.into());
        bytes.push(value.charset_family.into());
        bytes.push(value.size_of_char);
        bytes.push(value.reserved_byte);
        bytes.extend_from_slice(&value.data_format);
        bytes.extend_from_slice(&<[u8; 4]>::from(value.format_version));
        bytes.extend_from_slice(&value.data_version);

        bytes
    }
}

impl TryFrom<BinIndex> for Vec<u8> {
    type Error = BinarySerializerError;

    fn try_from(value: BinIndex) -> Result<Self, Self::Error> {
        let mut bytes =
            Vec::with_capacity(value.field_count as usize * core::mem::size_of::<u32>());

        // Format version 1.0 did not include an index and so no bytes should be
        // written.
        if value.field_count >= 5 {
            bytes.extend_from_slice(&value.field_count.to_ne_bytes());
            bytes.extend_from_slice(&value.keys_end.to_ne_bytes());
            bytes.extend_from_slice(&value.resources_end.to_ne_bytes());
            bytes.extend_from_slice(&value.bundle_end.to_ne_bytes());
            bytes.extend_from_slice(&value.largest_table_entry_count.to_ne_bytes());
        }

        if value.field_count >= 6 {
            let bundle_attributes =
                value
                    .bundle_attributes
                    .ok_or(BinarySerializerError::unexpected(
                        "no bundle attributes field provided",
                    ))?;

            bytes.extend_from_slice(&bundle_attributes.to_ne_bytes());
        }

        if value.field_count >= 7 {
            let data_16_bit_end =
                value
                    .data_16_bit_end
                    .ok_or(BinarySerializerError::unexpected(
                        "no 16-bit data end offset provided",
                    ))?;

            bytes.extend_from_slice(&data_16_bit_end.to_ne_bytes());
        }

        if value.field_count >= 8 {
            let pool_checksum = value
                .pool_checksum
                .ok_or(BinarySerializerError::unexpected(
                    "no pool checksum provided",
                ))?;

            bytes.extend_from_slice(&pool_checksum.to_ne_bytes());
        }

        Ok(bytes)
    }
}

impl From<FormatVersion> for [u8; 4] {
    fn from(value: FormatVersion) -> [u8; 4] {
        match value {
            FormatVersion::V1_0 => [1, 0, 0, 0],
            FormatVersion::V1_1 => [1, 1, 0, 0],
            FormatVersion::V1_2 => [1, 2, 0, 0],
            FormatVersion::V1_3 => [1, 3, 0, 0],
            FormatVersion::V2_0 => [2, 0, 0, 0],
            FormatVersion::V3_0 => [3, 0, 0, 0],
        }
    }
}

impl From<ResDescriptor> for u32 {
    fn from(value: ResDescriptor) -> Self {
        ((value.resource_type as u32) << 28) | value.value
    }
}

/// The `Error` type provides basic error handling for serialization of binary
/// resource bundles.
#[derive(Debug)]
pub struct BinarySerializerError {
    kind: ErrorKind,
}

impl BinarySerializerError {
    /// Creates a new error indicating that the input data contains a string
    /// longer than a binary resource bundle can encode.
    pub fn string_too_long(length: usize) -> Self {
        Self {
            kind: ErrorKind::StringTooLong(length),
        }
    }

    /// Creates a new error indicating that an unexpected error has occurred.
    ///
    /// Errors of this type should be programming errors within the writer,
    /// rather than problems with input data.
    pub fn unexpected(message: &'static str) -> Self {
        Self {
            kind: ErrorKind::Unexpected(message),
        }
    }
}

impl fmt::Display for BinarySerializerError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.kind {
            ErrorKind::StringTooLong(length) => write!(
                f,
                "Cannot serialize string of length {length}, maximum is 4,294,967,295 characters"
            ),
            ErrorKind::Unexpected(message) => write!(f, "Unexpected error: {message}"),
        }
    }
}

#[derive(Debug)]
enum ErrorKind {
    StringTooLong(usize),
    Unexpected(&'static str),
}
