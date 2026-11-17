/**
 * @file bacnet_encoding.h
 * @brief BACnet tag encoding/decoding helpers.
 *
 * Implements the subset of BACnet clause 20.2 needed by this device:
 * application tags (null, boolean, unsigned, signed, real, enumerated,
 * character string, bit string, object identifier) and context tags.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bacnet_types.h"

namespace bacnet {

/* ------------------------------------------------------------------------- */
/* Writer                                                                    */
/* ------------------------------------------------------------------------- */
class ByteWriter {
  public:
    ByteWriter(uint8_t *data, size_t capacity)
        : data_(data), capacity_(capacity), length_(0), overflow_(false)
    {
    }

    void reset()
    {
        length_ = 0;
        overflow_ = false;
    }

    size_t length() const { return length_; }
    bool overflow() const { return overflow_; }
    uint8_t *data() { return data_; }
    const uint8_t *data() const { return data_; }

    bool u8(uint8_t value);
    bool u16(uint16_t value);
    bool u32(uint32_t value);
    bool bytes(const uint8_t *data, size_t length);

    /* Low level tag header. lvt 0..4 = length, 5 = extended length,
     * 6 = opening tag, 7 = closing tag. */
    bool tagHeader(uint8_t tagNumber, bool context, uint8_t lvt);
    bool valueHeader(uint8_t tagNumber, bool context, size_t length);
    bool openingTag(uint8_t tagNumber) { return tagHeader(tagNumber, true, 6); }
    bool closingTag(uint8_t tagNumber) { return tagHeader(tagNumber, true, 7); }

    /* Application tagged values */
    bool appNull() { return tagHeader(0, false, 0); }
    bool appBoolean(bool value);
    bool appUnsigned(uint32_t value);
    bool appSigned(int32_t value);
    bool appReal(float value);
    bool appEnumerated(uint32_t value);
    bool appObjectId(uint16_t objectType, uint32_t instance);
    bool appString(const char *text);
    bool appBitString(uint8_t bitCount, const uint8_t *packedBits);

    /* Context tagged values (primitive) */
    bool ctxNull(uint8_t tagNumber)
    {
        return tagHeader(tagNumber, true, 0);
    }
    bool ctxUnsigned(uint8_t tagNumber, uint32_t value);
    bool ctxEnumerated(uint8_t tagNumber, uint32_t value);
    bool ctxObjectId(uint8_t tagNumber, uint16_t objectType, uint32_t instance);

  private:
    uint8_t *data_;
    size_t capacity_;
    size_t length_;
    bool overflow_;
};

/* ------------------------------------------------------------------------- */
/* Reader                                                                    */
/* ------------------------------------------------------------------------- */
class ByteReader {
  public:
    ByteReader(const uint8_t *data, size_t length)
        : data_(data), length_(length), position_(0)
    {
    }

    size_t position() const { return position_; }
    size_t remaining() const { return length_ - position_; }
    const uint8_t *current() const { return data_ + position_; }

    bool readU8(uint8_t &value);
    bool readU16(uint16_t &value);
    bool readU32(uint32_t &value);
    bool readBytes(uint8_t *out, size_t count);
    bool skip(size_t count);

    bool readTag(uint8_t &tagNumber,
                 bool &context,
                 bool &constructed,
                 bool &opening,
                 bool &closing,
                 uint32_t &length);

    /* Read a tag without advancing the reader. */
    bool peekTag(uint8_t &tagNumber,
                 bool &context,
                 bool &constructed,
                 bool &opening,
                 bool &closing,
                 uint32_t &length);

  private:
    const uint8_t *data_;
    size_t length_;
    size_t position_;
};

/* Decode one application tagged value from the reader. */
bool decodeApplicationValue(ByteReader &reader, Value &value);

/* ------------------------------------------------------------------------- */
/* Convenience helpers                                                       */
/* ------------------------------------------------------------------------- */
uint32_t makeObjectId(uint16_t objectType, uint32_t instance);
void splitObjectId(uint32_t objectId, uint16_t &objectType, uint32_t &instance);

} // namespace bacnet
