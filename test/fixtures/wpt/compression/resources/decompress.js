/**
 * @param {Uint8Array} chunk
 * @param {string} format
 */
async function decompressData(chunk, format) {
  const ds = new DecompressionStream(format);
  const writer = ds.writable.getWriter();
  writer.write(chunk);
  writer.close();
  const decompressedChunkList = await Array.fromAsync(ds.readable);
  const mergedBlob = new Blob(decompressedChunkList);
  return await mergedBlob.bytes();
}

/**
 * @param {Uint8Array} chunk
 * @param {string} format
 */
async function decompressDataOrPako(chunk, format) {
  // Keep using pako for zlib to preserve existing test behavior
  if (["deflate", "gzip"].includes(format)) {
    return pako.inflate(chunk);
  }
  if (format === "deflate-raw") {
    return pako.inflateRaw(chunk);
  }

  // Use DecompressionStream for any newer formats, assuming implementations
  // always implement decompression if they implement compression.
  return decompressData(chunk, format);
}
