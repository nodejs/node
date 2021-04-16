/*
 * buffer.h -- generic memory buffer.
 *
 * Copyright (c) 2005-2008, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 *
 * The buffer module implements a generic buffer.  The API is based on
 * the java.nio.Buffer interface.
 */

#ifndef GLDNS_SBUFFER_H
#define GLDNS_SBUFFER_H

#include <stdint.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef S_SPLINT_S
#  define INLINE 
#else
#  ifdef SWIG
#    define INLINE static
#  else
#    define INLINE static inline
#  endif
#endif

/*
 * Copy data allowing for unaligned accesses in network byte order
 * (big endian).
 */
INLINE uint16_t
gldns_read_uint16(const void *src)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
        return ntohs(*(const uint16_t *) src);
#else
        const uint8_t *p = (const uint8_t *) src;
        return ((uint16_t) p[0] << 8) | (uint16_t) p[1];
#endif
}

INLINE uint32_t
gldns_read_uint32(const void *src)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
        return ntohl(*(const uint32_t *) src);
#else
        const uint8_t *p = (const uint8_t *) src;
        return (  ((uint32_t) p[0] << 24)
                | ((uint32_t) p[1] << 16)
                | ((uint32_t) p[2] << 8)
                |  (uint32_t) p[3]);
#endif
}

/*
 * Copy data allowing for unaligned accesses in network byte order
 * (big endian).
 */
INLINE void
gldns_write_uint16(void *dst, uint16_t data)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
        * (uint16_t *) dst = htons(data);
#else
        uint8_t *p = (uint8_t *) dst;
        p[0] = (uint8_t) ((data >> 8) & 0xff);
        p[1] = (uint8_t) (data & 0xff);
#endif
}

INLINE void
gldns_write_uint32(void *dst, uint32_t data)
{
#ifdef ALLOW_UNALIGNED_ACCESSES
        * (uint32_t *) dst = htonl(data);
#else
        uint8_t *p = (uint8_t *) dst;
        p[0] = (uint8_t) ((data >> 24) & 0xff);
        p[1] = (uint8_t) ((data >> 16) & 0xff);
        p[2] = (uint8_t) ((data >> 8) & 0xff);
        p[3] = (uint8_t) (data & 0xff);
#endif
}


INLINE void
gldns_write_uint48(void *dst, uint64_t data)
{
        uint8_t *p = (uint8_t *) dst;
        p[0] = (uint8_t) ((data >> 40) & 0xff);
        p[1] = (uint8_t) ((data >> 32) & 0xff);
        p[2] = (uint8_t) ((data >> 24) & 0xff);
        p[3] = (uint8_t) ((data >> 16) & 0xff);
        p[4] = (uint8_t) ((data >> 8) & 0xff);
        p[5] = (uint8_t) (data & 0xff);
}


/**
 * \file gbuffer.h
 *
 * This file contains the definition of gldns_buffer, and functions to manipulate those.
 */

/** 
 * implementation of buffers to ease operations
 *
 * gldns_buffers can contain arbitrary information, per octet. You can write
 * to the current end of a buffer, read from the current position, and
 * access any data within it.
 */
struct gldns_buffer
{
	/** The current position used for reading/writing */ 
	size_t   _position;

	/** The read/write limit */
	size_t   _limit;

	/** The amount of data the buffer can contain */
	size_t   _capacity;

	/** The data contained in the buffer */
	uint8_t *_data;

	/** If the buffer is fixed it cannot be resized */
	unsigned _fixed : 1;

	/** If the buffer is vfixed, no more than capacity bytes will be
	 * written to _data, however the _position counter will be updated
	 * with the amount that would have been written in consecutive
	 * writes.  This allows for a modus operandi in which a sequence is
	 * written on a fixed capacity buffer (perhaps with _data on stack).
	 * When everything could be written, then the _data is immediately
	 * usable, if not, then a buffer could be allocated sized precisely
	 * to fit the data for a second attempt.
	 */
	unsigned _vfixed : 1;

	/** The current state of the buffer. If writing to the buffer fails
	 * for any reason, this value is changed. This way, you can perform
	 * multiple writes in sequence and check for success afterwards. */
	unsigned _status_err : 1;
};
typedef struct gldns_buffer gldns_buffer;

#ifdef NDEBUG
INLINE void
gldns_buffer_invariant(gldns_buffer *ATTR_UNUSED(buffer))
{
}
#else
INLINE void
gldns_buffer_invariant(gldns_buffer *buffer)
{
	assert(buffer != NULL);
	assert(buffer->_position <= buffer->_limit || buffer->_vfixed);
	assert(buffer->_limit <= buffer->_capacity);
	assert(buffer->_data != NULL || (buffer->_vfixed && buffer->_capacity == 0 && buffer->_limit == 0));
}
#endif

/**
 * creates a new buffer with the specified capacity.
 *
 * \param[in] capacity the size (in bytes) to allocate for the buffer
 * \return the created buffer
 */
gldns_buffer *gldns_buffer_new(size_t capacity);

/**
 * creates a buffer with the specified data.  The data IS copied
 * and MEMORY allocations are done.  The buffer is not fixed and can
 * be resized using buffer_reserve().
 *
 * \param[in] buffer pointer to the buffer to put the data in
 * \param[in] data the data to encapsulate in the buffer
 * \param[in] size the size of the data
 */
void gldns_buffer_new_frm_data(gldns_buffer *buffer, void *data, size_t size);

/**
 * Setup a buffer with the data pointed to. No data copied, no memory allocs.
 * The buffer is fixed.
 * \param[in] buffer pointer to the buffer to put the data in
 * \param[in] data the data to encapsulate in the buffer
 * \param[in] size the size of the data
 */
void gldns_buffer_init_frm_data(gldns_buffer *buffer, void *data, size_t size);

/**
 * Setup a buffer with the data pointed to. No data copied, no memory allocs.
 * The buffer is "virtually" fixed.  Writes beyond size (the capacity) will
 * only update position, but no data will be written beyond capacity.  This
 * allows to determine how big the buffer should have been to contain all the
 * written data, by looking at the position with gldns_buffer_position(),
 * similarly to the return value of POSIX's snprintf.
 * \param[in] buffer pointer to the buffer to put the data in
 * \param[in] data the data to encapsulate in the buffer
 * \param[in] size the size of the data
 */
void gldns_buffer_init_vfixed_frm_data(gldns_buffer *buffer, void *data, size_t size);

/**
 * clears the buffer and make it ready for writing.  The buffer's limit
 * is set to the capacity and the position is set to 0.
 * \param[in] buffer the buffer to clear
 */
INLINE void gldns_buffer_clear(gldns_buffer *buffer)
{
	gldns_buffer_invariant(buffer);

	/* reset status here? */

	buffer->_position = 0;
	buffer->_limit = buffer->_capacity;
}

/**
 * makes the buffer ready for reading the data that has been written to
 * the buffer.  The buffer's limit is set to the current position and
 * the position is set to 0.
 *
 * \param[in] buffer the buffer to flip
 * \return void
 */
INLINE void gldns_buffer_flip(gldns_buffer *buffer)
{
	gldns_buffer_invariant(buffer);

	buffer->_limit = buffer->_position;
	buffer->_position = 0;
}

/**
 * make the buffer ready for re-reading the data.  The buffer's
 * position is reset to 0.
 * \param[in] buffer the buffer to rewind
 */
INLINE void gldns_buffer_rewind(gldns_buffer *buffer)
{
	gldns_buffer_invariant(buffer);

	buffer->_position = 0;
}

/**
 * returns the current position in the buffer (as a number of bytes)
 * \param[in] buffer the buffer
 * \return the current position
 */
INLINE size_t
gldns_buffer_position(gldns_buffer *buffer)
{
	return buffer->_position;
}

/**
 * sets the buffer's position to MARK.  The position must be less than
 * or equal to the buffer's limit.
 * \param[in] buffer the buffer
 * \param[in] mark the mark to use
 */
INLINE void
gldns_buffer_set_position(gldns_buffer *buffer, size_t mark)
{
	assert(mark <= buffer->_limit || buffer->_vfixed);
	buffer->_position = mark;
}

/**
 * changes the buffer's position by COUNT bytes.  The position must not
 * be moved behind the buffer's limit or before the beginning of the
 * buffer.
 * \param[in] buffer the buffer
 * \param[in] count the count to use
 */
INLINE void
gldns_buffer_skip(gldns_buffer *buffer, ssize_t count)
{
	assert(buffer->_position + count <= buffer->_limit || buffer->_vfixed);
	buffer->_position += count;
}

/**
 * returns the maximum size of the buffer
 * \param[in] buffer
 * \return the size
 */
INLINE size_t
gldns_buffer_limit(gldns_buffer *buffer)
{
	return buffer->_limit;
}

/**
 * changes the buffer's limit.  If the buffer's position is greater
 * than the new limit the position is set to the limit.
 * \param[in] buffer the buffer
 * \param[in] limit the new limit
 */
INLINE void
gldns_buffer_set_limit(gldns_buffer *buffer, size_t limit)
{
	assert(limit <= buffer->_capacity);
	buffer->_limit = limit;
	if (buffer->_position > buffer->_limit)
		buffer->_position = buffer->_limit;
}

/**
 * returns the number of bytes the buffer can hold.
 * \param[in] buffer the buffer
 * \return the number of bytes
 */
INLINE size_t
gldns_buffer_capacity(gldns_buffer *buffer)
{
	return buffer->_capacity;
}

/**
 * changes the buffer's capacity.  The data is reallocated so any
 * pointers to the data may become invalid.  The buffer's limit is set
 * to the buffer's new capacity.
 * \param[in] buffer the buffer
 * \param[in] capacity the capacity to use
 * \return whether this failed or succeeded
 */
int gldns_buffer_set_capacity(gldns_buffer *buffer, size_t capacity);

/**
 * ensures BUFFER can contain at least AMOUNT more bytes.  The buffer's
 * capacity is increased if necessary using buffer_set_capacity().
 *
 * The buffer's limit is always set to the (possibly increased)
 * capacity.
 * \param[in] buffer the buffer
 * \param[in] amount amount to use
 * \return whether this failed or succeeded
 */
int gldns_buffer_reserve(gldns_buffer *buffer, size_t amount);

/**
 * returns a pointer to the data at the indicated position.
 * \param[in] buffer the buffer
 * \param[in] at position
 * \return the pointer to the data
 */
INLINE uint8_t *
gldns_buffer_at(const gldns_buffer *buffer, size_t at)
{
	assert(at <= buffer->_limit || buffer->_vfixed);
	return buffer->_data + at;
}

/**
 * returns a pointer to the beginning of the buffer (the data at
 * position 0).
 * \param[in] buffer the buffer
 * \return the pointer
 */
INLINE uint8_t *
gldns_buffer_begin(const gldns_buffer *buffer)
{
	return gldns_buffer_at(buffer, 0);
}

/**
 * returns a pointer to the end of the buffer (the data at the buffer's
 * limit).
 * \param[in] buffer the buffer
 * \return the pointer
 */
INLINE uint8_t *
gldns_buffer_end(gldns_buffer *buffer)
{
	return gldns_buffer_at(buffer, buffer->_limit);
}

/**
 * returns a pointer to the data at the buffer's current position.
 * \param[in] buffer the buffer
 * \return the pointer
 */
INLINE uint8_t *
gldns_buffer_current(gldns_buffer *buffer)
{
	return gldns_buffer_at(buffer, buffer->_position);
}

/**
 * returns the number of bytes remaining between the indicated position and
 * the limit.
 * \param[in] buffer the buffer
 * \param[in] at indicated position
 * \return number of bytes
 */
INLINE size_t
gldns_buffer_remaining_at(gldns_buffer *buffer, size_t at)
{
	gldns_buffer_invariant(buffer);
	assert(at <= buffer->_limit || buffer->_vfixed);
	return at < buffer->_limit ? buffer->_limit - at : 0;
}

/**
 * returns the number of bytes remaining between the buffer's position and
 * limit.
 * \param[in] buffer the buffer
 * \return the number of bytes
 */
INLINE size_t
gldns_buffer_remaining(gldns_buffer *buffer)
{
	return gldns_buffer_remaining_at(buffer, buffer->_position);
}

/**
 * checks if the buffer has at least COUNT more bytes available.
 * Before reading or writing the caller needs to ensure enough space
 * is available!
 * \param[in] buffer the buffer
 * \param[in] at indicated position
 * \param[in] count how much is available
 * \return true or false (as int?)
 */
INLINE int
gldns_buffer_available_at(gldns_buffer *buffer, size_t at, size_t count)
{
	return count <= gldns_buffer_remaining_at(buffer, at);
}

/**
 * checks if the buffer has count bytes available at the current position
 * \param[in] buffer the buffer
 * \param[in] count how much is available
 * \return true or false (as int?)
 */
INLINE int
gldns_buffer_available(gldns_buffer *buffer, size_t count)
{
	return gldns_buffer_available_at(buffer, buffer->_position, count);
}

/**
 * writes the given data to the buffer at the specified position
 * \param[in] buffer the buffer
 * \param[in] at the position (in number of bytes) to write the data at
 * \param[in] data pointer to the data to write to the buffer
 * \param[in] count the number of bytes of data to write
 */
INLINE void
gldns_buffer_write_at(gldns_buffer *buffer, size_t at, const void *data, size_t count)
{
	if (!buffer->_vfixed)
		assert(gldns_buffer_available_at(buffer, at, count));
	else if (gldns_buffer_remaining_at(buffer, at) == 0)
		return;
	else if (count > gldns_buffer_remaining_at(buffer, at)) {
		memcpy(buffer->_data + at, data,
		    gldns_buffer_remaining_at(buffer, at));
		return;
	}
	memcpy(buffer->_data + at, data, count);
}

/**
 * set the given byte to the buffer at the specified position
 * \param[in] buffer the buffer
 * \param[in] at the position (in number of bytes) to write the data at
 * \param[in] c the byte to set to the buffer
 * \param[in] count the number of bytes of bytes to write
 */

INLINE void
gldns_buffer_set_at(gldns_buffer *buffer, size_t at, int c, size_t count)
{
    if (!buffer->_vfixed)
        assert(gldns_buffer_available_at(buffer, at, count));
    else if (gldns_buffer_remaining_at(buffer, at) == 0)
        return;
    else if (count > gldns_buffer_remaining_at(buffer, at)) {
        memset(buffer->_data + at, c,
            gldns_buffer_remaining_at(buffer, at));
        return;
    }
	memset(buffer->_data + at, c, count);
}


/**
 * writes count bytes of data to the current position of the buffer
 * \param[in] buffer the buffer
 * \param[in] data the data to write
 * \param[in] count the length of the data to write
 */
INLINE void
gldns_buffer_write(gldns_buffer *buffer, const void *data, size_t count)
{
	gldns_buffer_write_at(buffer, buffer->_position, data, count);
	buffer->_position += count;
}

/**
 * copies the given (null-delimited) string to the specified position at the buffer
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer
 * \param[in] str the string to write
 */
INLINE void
gldns_buffer_write_string_at(gldns_buffer *buffer, size_t at, const char *str)
{
	gldns_buffer_write_at(buffer, at, str, strlen(str));
}

/**
 * copies the given (null-delimited) string to the current position at the buffer
 * \param[in] buffer the buffer
 * \param[in] str the string to write
 */
INLINE void
gldns_buffer_write_string(gldns_buffer *buffer, const char *str)
{
	gldns_buffer_write(buffer, str, strlen(str));
}

/**
 * writes the given byte of data at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer
 * \param[in] data the 8 bits to write
 */
INLINE void
gldns_buffer_write_u8_at(gldns_buffer *buffer, size_t at, uint8_t data)
{
	if (buffer->_vfixed && at + sizeof(data) > buffer->_limit) return;
	assert(gldns_buffer_available_at(buffer, at, sizeof(data)));
	buffer->_data[at] = data;
}

/**
 * writes the given byte of data at the current position in the buffer
 * \param[in] buffer the buffer
 * \param[in] data the 8 bits to write
 */
INLINE void
gldns_buffer_write_u8(gldns_buffer *buffer, uint8_t data)
{
	gldns_buffer_write_u8_at(buffer, buffer->_position, data);
	buffer->_position += sizeof(data);
}

/**
 * writes the given 2 byte integer at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer
 * \param[in] data the 16 bits to write
 */
INLINE void
gldns_buffer_write_u16_at(gldns_buffer *buffer, size_t at, uint16_t data)
{
	if (buffer->_vfixed && at + sizeof(data) > buffer->_limit) return;
	assert(gldns_buffer_available_at(buffer, at, sizeof(data)));
	gldns_write_uint16(buffer->_data + at, data);
}

/**
 * writes the given 2 byte integer at the current position in the buffer
 * \param[in] buffer the buffer
 * \param[in] data the 16 bits to write
 */
INLINE void
gldns_buffer_write_u16(gldns_buffer *buffer, uint16_t data)
{
	gldns_buffer_write_u16_at(buffer, buffer->_position, data);
	buffer->_position += sizeof(data);
}

/**
 * writes the given 4 byte integer at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer
 * \param[in] data the 32 bits to write
 */
INLINE void
gldns_buffer_write_u32_at(gldns_buffer *buffer, size_t at, uint32_t data)
{
	if (buffer->_vfixed && at + sizeof(data) > buffer->_limit) return;
	assert(gldns_buffer_available_at(buffer, at, sizeof(data)));
	gldns_write_uint32(buffer->_data + at, data);
}

/**
 * writes the given 6 byte integer at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer
 * \param[in] data the (lower) 48 bits to write
 */
INLINE void
gldns_buffer_write_u48_at(gldns_buffer *buffer, size_t at, uint64_t data)
{
	if (buffer->_vfixed && at + 6 > buffer->_limit) return;
	assert(gldns_buffer_available_at(buffer, at, 6));
	gldns_write_uint48(buffer->_data + at, data);
}

/**
 * writes the given 4 byte integer at the current position in the buffer
 * \param[in] buffer the buffer
 * \param[in] data the 32 bits to write
 */
INLINE void
gldns_buffer_write_u32(gldns_buffer *buffer, uint32_t data)
{
	gldns_buffer_write_u32_at(buffer, buffer->_position, data);
	buffer->_position += sizeof(data);
}

/**
 * writes the given 6 byte integer at the current position in the buffer
 * \param[in] buffer the buffer
 * \param[in] data the 48 bits to write
 */
INLINE void
gldns_buffer_write_u48(gldns_buffer *buffer, uint64_t data)
{
	gldns_buffer_write_u48_at(buffer, buffer->_position, data);
	buffer->_position += 6;
}

/**
 * copies count bytes of data at the given position to the given data-array
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer to start
 * \param[out] data buffer to copy to
 * \param[in] count the length of the data to copy
 */
INLINE void
gldns_buffer_read_at(gldns_buffer *buffer, size_t at, void *data, size_t count)
{
	assert(gldns_buffer_available_at(buffer, at, count));
	memcpy(data, buffer->_data + at, count);
}

/**
 * copies count bytes of data at the current position to the given data-array
 * \param[in] buffer the buffer
 * \param[out] data buffer to copy to
 * \param[in] count the length of the data to copy
 */
INLINE void
gldns_buffer_read(gldns_buffer *buffer, void *data, size_t count)
{
	gldns_buffer_read_at(buffer, buffer->_position, data, count);
	buffer->_position += count;
}

/**
 * returns the byte value at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at the position in the buffer
 * \return 1 byte integer
 */
INLINE uint8_t
gldns_buffer_read_u8_at(gldns_buffer *buffer, size_t at)
{
	assert(gldns_buffer_available_at(buffer, at, sizeof(uint8_t)));
	return buffer->_data[at];
}

/**
 * returns the byte value at the current position in the buffer
 * \param[in] buffer the buffer
 * \return 1 byte integer
 */
INLINE uint8_t
gldns_buffer_read_u8(gldns_buffer *buffer)
{
	uint8_t result = gldns_buffer_read_u8_at(buffer, buffer->_position);
	buffer->_position += sizeof(uint8_t);
	return result;
}

/**
 * returns the 2-byte integer value at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at position in the buffer
 * \return 2 byte integer
 */
INLINE uint16_t
gldns_buffer_read_u16_at(gldns_buffer *buffer, size_t at)
{
	assert(gldns_buffer_available_at(buffer, at, sizeof(uint16_t)));
	return gldns_read_uint16(buffer->_data + at);
}

/**
 * returns the 2-byte integer value at the current position in the buffer
 * \param[in] buffer the buffer
 * \return 2 byte integer
 */
INLINE uint16_t
gldns_buffer_read_u16(gldns_buffer *buffer)
{
	uint16_t result = gldns_buffer_read_u16_at(buffer, buffer->_position);
	buffer->_position += sizeof(uint16_t);
	return result;
}

/**
 * returns the 4-byte integer value at the given position in the buffer
 * \param[in] buffer the buffer
 * \param[in] at position in the buffer
 * \return 4 byte integer
 */
INLINE uint32_t
gldns_buffer_read_u32_at(gldns_buffer *buffer, size_t at)
{
	assert(gldns_buffer_available_at(buffer, at, sizeof(uint32_t)));
	return gldns_read_uint32(buffer->_data + at);
}

/**
 * returns the 4-byte integer value at the current position in the buffer
 * \param[in] buffer the buffer
 * \return 4 byte integer
 */
INLINE uint32_t
gldns_buffer_read_u32(gldns_buffer *buffer)
{
	uint32_t result = gldns_buffer_read_u32_at(buffer, buffer->_position);
	buffer->_position += sizeof(uint32_t);
	return result;
}

/**
 * returns the status of the buffer
 * \param[in] buffer
 * \return the status
 */
INLINE int
gldns_buffer_status(gldns_buffer *buffer)
{
	return (int)buffer->_status_err;
}

/**
 * returns true if the status of the buffer is GLDNS_STATUS_OK, false otherwise
 * \param[in] buffer the buffer
 * \return true or false
 */
INLINE int
gldns_buffer_status_ok(gldns_buffer *buffer)
{
	if (buffer) {
		return gldns_buffer_status(buffer) == 0;
	} else {
		return 0;
	}
}

/**
 * prints to the buffer, increasing the capacity if required using
 * buffer_reserve(). The buffer's position is set to the terminating '\\0'
 * Returns the number of characters written (not including the
 * terminating '\\0') or -1 on failure.
 */
int gldns_buffer_printf(gldns_buffer *buffer, const char *format, ...)
	ATTR_FORMAT(printf, 2, 3);

/**
 * frees the buffer.
 * \param[in] *buffer the buffer to be freed
 * \return void
 */
void gldns_buffer_free(gldns_buffer *buffer);

/**
 * Makes the buffer fixed and returns a pointer to the data.  The
 * caller is responsible for free'ing the result.
 * \param[in] *buffer the buffer to be exported
 * \return void
 */
void *gldns_buffer_export(gldns_buffer *buffer);

/**
 * Copy contents of the from buffer to the result buffer and then flips 
 * the result buffer. Data will be silently truncated if the result buffer is
 * too small.
 * \param[out] *result resulting buffer which is copied to.
 * \param[in] *from what to copy to result.
 */
void gldns_buffer_copy(gldns_buffer* result, gldns_buffer* from);

#ifdef __cplusplus
}
#endif

#endif /* GLDNS_SBUFFER_H */
