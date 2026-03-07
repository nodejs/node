use std::any::type_name;

use anyhow::Error;

#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
#[cfg_attr(
    feature = "__plugin",
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "__plugin", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "__plugin", repr(u32))]
/// Enum for possible errors while running transform via plugin.
///
/// This error indicates internal operation failure either in plugin_runner
/// or plugin_macro. Plugin's transform fn itself does not allow to return
/// error - instead it should use provided `handler` to emit corresponding error
/// to the host.
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
pub enum PluginError {
    /// Occurs when failed to convert size passed from host / guest into usize
    /// or similar for the conversion. This is an internal error rasied via
    /// plugin_macro, normally plugin author should not raise this manually.
    SizeInteropFailure(String),
    /// Occurs when failed to reconstruct a struct from `Serialized`.
    Deserialize(String),
    /// Occurs when failed to serialize a struct into `Serialized`.
    /// Unlike deserialize error, this error cannot forward any context for the
    /// raw bytes: when serialize failed, there's nothing we can pass between
    /// runtime.
    Serialize(String),
}

/// A wrapper type for the internal representation of serialized data.
///
/// Wraps internal representation of serialized data for exchanging data between
/// plugin to the host. Consumers should not rely on specific details of byte
/// format struct contains: it is strict implementation detail which can
/// change anytime.
pub struct PluginSerializedBytes {
    pub(crate) field: Vec<u8>,
}

#[cfg(feature = "__plugin")]
impl PluginSerializedBytes {
    /**
     * Constructs an instance from already serialized byte
     * slices.
     */
    #[tracing::instrument(level = "info", skip_all)]
    pub fn from_bytes(field: Vec<u8>) -> PluginSerializedBytes {
        PluginSerializedBytes { field }
    }

    /**
     * Constructs an instance from versioned struct by serializing it.
     *
     * This is sort of mimic TryFrom behavior, since we can't use generic
     * to implement TryFrom trait
     */
    /*
    #[tracing::instrument(level = "info", skip_all)]
    pub fn try_serialize<W>(t: &VersionedSerializable<W>) -> Result<Self, Error>
    where
        W: rkyv::Serialize<rkyv::ser::serializers::AllocSerializer<512>>,
    {
        rkyv::to_bytes::<_, 512>(t)
            .map(|field| PluginSerializedBytes { field })
            .map_err(|err| match err {
                rkyv::ser::serializers::CompositeSerializerError::SerializerError(e) => e.into(),
                rkyv::ser::serializers::CompositeSerializerError::ScratchSpaceError(_e) => {
                    Error::msg("AllocScratchError")
                }
                rkyv::ser::serializers::CompositeSerializerError::SharedError(_e) => {
                    Error::msg("SharedSerializeMapError")
                }
            })
    }
     */

    #[tracing::instrument(level = "info", skip_all)]
    pub fn try_serialize<W>(t: &VersionedSerializable<W>) -> Result<Self, Error>
    where
        W: cbor4ii::core::enc::Encode,
    {
        let mut buf = cbor4ii::core::utils::BufWriter::new(Vec::new());
        t.0.encode(&mut buf)?;
        Ok(PluginSerializedBytes {
            field: buf.into_inner(),
        })
    }

    /*
     * Internal fn to constructs an instance from raw bytes ptr.
     */
    #[tracing::instrument(level = "info", skip_all)]
    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    pub fn from_raw_ptr(
        raw_allocated_ptr: *const u8,
        raw_allocated_ptr_len: usize,
    ) -> PluginSerializedBytes {
        let raw_ptr_bytes =
            unsafe { std::slice::from_raw_parts(raw_allocated_ptr, raw_allocated_ptr_len) };

        PluginSerializedBytes::from_bytes(raw_ptr_bytes.to_vec())
    }

    pub fn as_slice(&self) -> &[u8] {
        self.field.as_slice()
    }

    pub fn as_ptr(&self) -> (*const u8, usize) {
        (self.field.as_ptr(), self.field.len())
    }

    #[tracing::instrument(level = "info", skip_all)]
    pub fn deserialize<W>(&self) -> Result<VersionedSerializable<W>, Error>
    where
        W: for<'de> cbor4ii::core::dec::Decode<'de>,
    {
        use anyhow::Context;

        let mut buf = cbor4ii::core::utils::SliceReader::new(&self.field);
        let deserialized = <W>::decode(&mut buf)
            .with_context(|| format!("failed to deserialize `{}`", type_name::<W>()))?;
        Ok(VersionedSerializable(deserialized))
    }

    /*
    #[tracing::instrument(level = "info", skip_all)]
    pub fn deserialize<W>(&self) -> Result<VersionedSerializable<W>, Error>
    where
        W: rkyv::Archive,
        W::Archived: rkyv::Deserialize<W, rkyv::de::deserializers::SharedDeserializeMap>
            + for<'a> rkyv::CheckBytes<rkyv::validation::validators::DefaultValidator<'a>>,
    {
        use anyhow::Context;

        let archived = rkyv::check_archived_root::<VersionedSerializable<W>>(&self.field[..])
            .map_err(|err| {
                anyhow::format_err!("wasm plugin bytecheck failed {:?}", err.to_string())
            })?;

        archived
            .deserialize(&mut rkyv::de::deserializers::SharedDeserializeMap::new())
            .with_context(|| format!("failed to deserialize `{}`", type_name::<W>()))
    } */
}

/// A wrapper type for the structures to be passed into plugins
/// serializes the contained value out-of-line so that newer
/// versions can be viewed as the older version.
///
/// First field indicate version of struct type (schema). Any consumers like
/// swc_plugin_macro can use this to validate compatiblility before attempt to
/// serialize.
#[cfg_attr(
    feature = "__plugin",
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[repr(transparent)]
#[cfg_attr(feature = "__plugin", derive(bytecheck::CheckBytes))]
#[derive(Debug)]
pub struct VersionedSerializable<T>(
    // [NOTE]: https://github.com/rkyv/rkyv/issues/373#issuecomment-1546360897
    //#[cfg_attr(feature = "__plugin", with(rkyv::with::AsBox))]
    pub T,
);

impl<T> VersionedSerializable<T> {
    pub fn new(value: T) -> Self {
        Self(value)
    }

    pub fn inner(&self) -> &T {
        &self.0
    }

    pub fn into_inner(self) -> T {
        self.0
    }
}

pub struct ResultValue<R, E>(pub Result<R, E>);

#[cfg(feature = "encoding-impl")]
impl<R, E> cbor4ii::core::enc::Encode for ResultValue<R, E>
where
    R: cbor4ii::core::enc::Encode,
    E: cbor4ii::core::enc::Encode,
{
    #[inline]
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        use cbor4ii::core::types::Tag;

        match &self.0 {
            Ok(t) => Tag(1, t).encode(writer),
            Err(t) => Tag(0, t).encode(writer),
        }
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de, T, E> cbor4ii::core::dec::Decode<'de> for ResultValue<T, E>
where
    T: cbor4ii::core::dec::Decode<'de>,
    E: cbor4ii::core::dec::Decode<'de>,
{
    #[inline]
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        let tag = cbor4ii::core::types::Tag::tag(reader)?;
        match tag {
            0 => E::decode(reader).map(Result::Err).map(ResultValue),
            1 => T::decode(reader).map(Result::Ok).map(ResultValue),
            _ => Err(cbor4ii::core::error::DecodeError::Mismatch {
                name: &"ResultValue",
                found: tag.to_le_bytes()[0],
            }),
        }
    }
}
