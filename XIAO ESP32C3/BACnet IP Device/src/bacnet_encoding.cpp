/**
 * @file bacnet_encoding.cpp
 * @brief BACnet tag encoding/decoding helpers.
 */
#include "bacnet_encoding.h"

#include <string.h>

namespace bacnet {

/* ------------------------------------------------------------------------- */
/* ByteWriter                                                                */
/* ------------------------------------------------------------------------- */
bool ByteWriter::u8(uint8_t value)
{
    if (length_ + 1 > capacity_) {
        overflow_ = true;
        return false;
    }
    data_[length_++] = value;
    return true;
}

bool ByteWriter::u16(uint16_t value)
{
    return u8(static_cast<uint8_t>(value >> 8)) &&
           u8(static_cast<uint8_t>(value & 0xFF));
}

bool ByteWriter::u32(uint32_t value)
{
    return u8(static_cast<uint8_t>((value >> 24) & 0xFF)) &&
           u8(static_cast<uint8_t>((value >> 16) & 0xFF)) &&
           u8(static_cast<uint8_t>((value >> 8) & 0xFF)) &&
           u8(static_cast<uint8_t>(value & 0xFF));
}

bool ByteWriter::bytes(const uint8_t *data, size_t length)
{
    if (length_ + length > capacity_) {
        overflow_ = true;
        return false;
    }
    if (data && length) {
        memcpy(data_ + length_, data, length);
    }
    length_ += length;
    return true;
}

bool ByteWriter::tagHeader(uint8_t tagNumber, bool context, uint8_t lvt)
{
    const uint8_t classBit = context ? 0x08 : 0x00;
    if (tagNumber <= 14) {
        return u8(static_cast<uint8_t>((tagNumber << 4) | classBit | (lvt & 0x07)));
    }
    if (!u8(static_cast<uint8_t>(0xF0 | classBit | (lvt & 0x07)))) {
        return false;
    }
    return u8(tagNumber);
}

bool ByteWriter::valueHeader(uint8_t tagNumber, bool context, size_t length)
{
    if (length <= 4) {
        return tagHeader(tagNumber, context, static_cast<uint8_t>(length));
    }
    if (length <= 0xFD) {
        return tagHeader(tagNumber, context, 5) &&
               u8(static_cast<uint8_t>(length));
    }
    if (length <= 0xFFFF) {
        return tagHeader(tagNumber, context, 5) && u8(0xFE) &&
               u16(static_cast<uint16_t>(length));
    }
    return tagHeader(tagNumber, context, 5) && u8(0xFF) &&
           u32(static_cast<uint32_t>(length));
}

bool ByteWriter::appBoolean(bool value)
{
    /* Clause 20.2.3: a Boolean application value has no contents octets -
     * the value itself is carried in the length/type field. */
    return tagHeader(1, false, value ? 1 : 0);
}

bool ByteWriter::appUnsigned(uint32_t value)
{
    uint8_t length = 1;
    if (value > 0xFFFFFFu) {
        length = 4;
    } else if (value > 0xFFFFu) {
        length = 3;
    } else if (value > 0xFFu) {
        length = 2;
    }
    if (!tagHeader(2, false, length)) {
        return false;
    }
    for (int i = length - 1; i >= 0; --i) {
        if (!u8(static_cast<uint8_t>((value >> (i * 8)) & 0xFF))) {
            return false;
        }
    }
    return true;
}

bool ByteWriter::appSigned(int32_t value)
{
    uint8_t length = 1;
    for (; length < 4; ++length) {
        const int64_t minValue = -(1LL << (length * 8 - 1));
        const int64_t maxValue = (1LL << (length * 8 - 1)) - 1;
        if (value >= minValue && value <= maxValue) {
            break;
        }
    }
    if (!tagHeader(3, false, length)) {
        return false;
    }
    const uint32_t raw = static_cast<uint32_t>(value);
    for (int i = length - 1; i >= 0; --i) {
        if (!u8(static_cast<uint8_t>((raw >> (i * 8)) & 0xFF))) {
            return false;
        }
    }
    return true;
}

bool ByteWriter::appReal(float value)
{
    uint32_t raw = 0;
    static_assert(sizeof(float) == 4, "BACnet REAL requires a 32-bit float");
    memcpy(&raw, &value, sizeof(raw));
    return tagHeader(4, false, 4) && u32(raw);
}

bool ByteWriter::appEnumerated(uint32_t value)
{
    uint8_t length = 1;
    if (value > 0xFFFFFFu) {
        length = 4;
    } else if (value > 0xFFFFu) {
        length = 3;
    } else if (value > 0xFFu) {
        length = 2;
    }
    if (!tagHeader(9, false, length)) {
        return false;
    }
    for (int i = length - 1; i >= 0; --i) {
        if (!u8(static_cast<uint8_t>((value >> (i * 8)) & 0xFF))) {
            return false;
        }
    }
    return true;
}

bool ByteWriter::appObjectId(uint16_t objectType, uint32_t instance)
{
    return tagHeader(12, false, 4) && u32(makeObjectId(objectType, instance));
}

bool ByteWriter::appString(const char *text)
{
    if (!text) {
        text = "";
    }
    const size_t textLength = strlen(text);
    /* One leading octet carries the BACnet character set (0 = UTF-8/ANSI). */
    if (!valueHeader(7, false, textLength + 1)) {
        return false;
    }
    if (!u8(0)) {
        return false;
    }
    return bytes(reinterpret_cast<const uint8_t *>(text), textLength);
}

bool ByteWriter::appBitString(uint8_t bitCount, const uint8_t *packedBits)
{
    /* The writer only packs bytes, so any bit string the caller supplies is
     * fine. protocol-services-supported needs up to 50 bits (Who-Is sits at
     * bit 34); the 4-byte limit only applies to the decoding Value struct. */
    if (bitCount > 64) {
        return false;
    }
    const uint8_t byteCount = static_cast<uint8_t>((bitCount + 7) / 8);
    const uint8_t unusedBits = static_cast<uint8_t>(byteCount * 8 - bitCount);
    if (!valueHeader(8, false, byteCount + 1)) {
        return false;
    }
    if (!u8(unusedBits)) {
        return false;
    }
    return bytes(packedBits, byteCount);
}

bool ByteWriter::ctxUnsigned(uint8_t tagNumber, uint32_t value)
{
    uint8_t length = 1;
    if (value > 0xFFFFFFu) {
        length = 4;
    } else if (value > 0xFFFFu) {
        length = 3;
    } else if (value > 0xFFu) {
        length = 2;
    }
    if (!tagHeader(tagNumber, true, length)) {
        return false;
    }
    for (int i = length - 1; i >= 0; --i) {
        if (!u8(static_cast<uint8_t>((value >> (i * 8)) & 0xFF))) {
            return false;
        }
    }
    return true;
}

bool ByteWriter::ctxEnumerated(uint8_t tagNumber, uint32_t value)
{
    return ctxUnsigned(tagNumber, value);
}

bool ByteWriter::ctxObjectId(uint8_t tagNumber,
                             uint16_t objectType,
                             uint32_t instance)
{
    return tagHeader(tagNumber, true, 4) &&
           u32(makeObjectId(objectType, instance));
}

/* ------------------------------------------------------------------------- */
/* ByteReader                                                                */
/* ------------------------------------------------------------------------- */
bool ByteReader::readU8(uint8_t &value)
{
    if (position_ + 1 > length_) {
        return false;
    }
    value = data_[position_++];
    return true;
}

bool ByteReader::readU16(uint16_t &value)
{
    uint8_t hi = 0;
    uint8_t lo = 0;
    if (!readU8(hi) || !readU8(lo)) {
        return false;
    }
    value = static_cast<uint16_t>((hi << 8) | lo);
    return true;
}

bool ByteReader::readU32(uint32_t &value)
{
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;
    if (!readU8(b0) || !readU8(b1) || !readU8(b2) || !readU8(b3)) {
        return false;
    }
    value = (static_cast<uint32_t>(b0) << 24) |
            (static_cast<uint32_t>(b1) << 16) |
            (static_cast<uint32_t>(b2) << 8) | static_cast<uint32_t>(b3);
    return true;
}

bool ByteReader::readBytes(uint8_t *out, size_t count)
{
    if (position_ + count > length_) {
        return false;
    }
    if (out && count) {
        memcpy(out, data_ + position_, count);
    }
    position_ += count;
    return true;
}

bool ByteReader::skip(size_t count)
{
    if (position_ + count > length_) {
        return false;
    }
    position_ += count;
    return true;
}

bool ByteReader::readTag(uint8_t &tagNumber,
                         bool &context,
                         bool &constructed,
                         bool &opening,
                         bool &closing,
                         uint32_t &length)
{
    uint8_t first = 0;
    if (!readU8(first)) {
        return false;
    }

    tagNumber = static_cast<uint8_t>(first >> 4);
    if (tagNumber == 0x0F) {
        uint8_t extended = 0;
        if (!readU8(extended)) {
            return false;
        }
        tagNumber = extended;
    }

    context = (first & 0x08) != 0;
    const uint8_t lvt = static_cast<uint8_t>(first & 0x07);
    constructed = false;
    opening = false;
    closing = false;
    length = 0;

    if (lvt <= 4) {
        length = lvt;
        return true;
    }
    if (lvt == 5) {
        uint8_t extendedLength = 0;
        if (!readU8(extendedLength)) {
            return false;
        }
        if (extendedLength == 0xFE) {
            uint16_t value16 = 0;
            if (!readU16(value16)) {
                return false;
            }
            length = value16;
        } else if (extendedLength == 0xFF) {
            uint32_t value32 = 0;
            if (!readU32(value32)) {
                return false;
            }
            length = value32;
        } else {
            length = extendedLength;
        }
        return true;
    }
    if (lvt == 6) {
        constructed = true;
        opening = true;
        return true;
    }
    /* lvt == 7 */
    constructed = true;
    closing = true;
    return true;
}

bool ByteReader::peekTag(uint8_t &tagNumber,
                         bool &context,
                         bool &constructed,
                         bool &opening,
                         bool &closing,
                         uint32_t &length)
{
    const size_t saved = position_;
    const bool ok = readTag(
        tagNumber, context, constructed, opening, closing, length);
    position_ = saved;
    return ok;
}

/* ------------------------------------------------------------------------- */
/* Application value decoding                                                */
/* ------------------------------------------------------------------------- */
bool decodeApplicationValue(ByteReader &reader, Value &value)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.readTag(tag, context, constructed, opening, closing, length)) {
        return false;
    }
    if (context || constructed || opening || closing) {
        return false;
    }

    value = Value();
    switch (tag) {
        case 0: /* NULL */
            return length == 0;

        case 1: /* BOOLEAN - clause 20.2.3: the LVT field is the value */
            value.type = Value::Boolean;
            value.boolean = (length != 0);
            return true;

        case 2: { /* UNSIGNED */
            if (length < 1 || length > 4) {
                return false;
            }
            uint32_t raw = 0;
            for (uint32_t i = 0; i < length; ++i) {
                uint8_t byte = 0;
                if (!reader.readU8(byte)) {
                    return false;
                }
                raw = (raw << 8) | byte;
            }
            value.type = Value::Unsigned;
            value.unsignedValue = raw;
            return true;
        }

        case 3: { /* SIGNED */
            if (length < 1 || length > 4) {
                return false;
            }
            uint32_t raw = 0;
            for (uint32_t i = 0; i < length; ++i) {
                uint8_t byte = 0;
                if (!reader.readU8(byte)) {
                    return false;
                }
                raw = (raw << 8) | byte;
            }
            if (length < 4 && (raw & (1u << (length * 8 - 1)))) {
                raw |= ~((1u << (length * 8)) - 1);
            }
            value.type = Value::Signed;
            value.signedValue = static_cast<int32_t>(raw);
            return true;
        }

        case 4: { /* REAL */
            uint32_t raw = 0;
            if (length != 4 || !reader.readU32(raw)) {
                return false;
            }
            float real = 0.0f;
            memcpy(&real, &raw, sizeof(real));
            value.type = Value::Real;
            value.real = real;
            return true;
        }

        case 7: { /* CHARACTER STRING */
            if (length < 1) {
                return false;
            }
            uint8_t characterSet = 0;
            if (!reader.readU8(characterSet)) {
                return false;
            }
            (void)characterSet;
            const uint32_t textLength = length - 1;
            const uint32_t copyLength = textLength > (sizeof(value.text) - 1)
                                            ? (sizeof(value.text) - 1)
                                            : textLength;
            if (!reader.readBytes(reinterpret_cast<uint8_t *>(value.text),
                                  copyLength)) {
                return false;
            }
            if (textLength > copyLength &&
                !reader.skip(textLength - copyLength)) {
                return false;
            }
            value.text[copyLength] = '\0';
            value.type = Value::CharacterString;
            return true;
        }

        case 8: { /* BIT STRING */
            if (length < 1) {
                return false;
            }
            uint8_t unusedBits = 0;
            if (!reader.readU8(unusedBits)) {
                return false;
            }
            uint32_t byteCount = length - 1;
            uint8_t temp[4] = {0, 0, 0, 0};
            const uint32_t copyLength = byteCount > 4 ? 4 : byteCount;
            if (!reader.readBytes(temp, copyLength)) {
                return false;
            }
            if (byteCount > copyLength && !reader.skip(byteCount - copyLength)) {
                return false;
            }
            if (byteCount > 4 || unusedBits > byteCount * 8) {
                return false;
            }
            value.type = Value::BitStringValue;
            value.bitString.bitCount =
                static_cast<uint8_t>(byteCount * 8 - unusedBits);
            memcpy(value.bitString.bytes, temp, sizeof(temp));
            return true;
        }

        case 9: { /* ENUMERATED */
            if (length < 1 || length > 4) {
                return false;
            }
            uint32_t raw = 0;
            for (uint32_t i = 0; i < length; ++i) {
                uint8_t byte = 0;
                if (!reader.readU8(byte)) {
                    return false;
                }
                raw = (raw << 8) | byte;
            }
            value.type = Value::Enumerated;
            value.unsignedValue = raw;
            return true;
        }

        case 12: { /* OBJECT IDENTIFIER */
            uint32_t raw = 0;
            if (length != 4 || !reader.readU32(raw)) {
                return false;
            }
            value.type = Value::ObjectIdentifier;
            splitObjectId(raw, value.objectType, value.objectInstance);
            return true;
        }

        default:
            return false;
    }
}

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */
uint32_t makeObjectId(uint16_t objectType, uint32_t instance)
{
    return ((static_cast<uint32_t>(objectType) & 0x3FFu) << 22) |
           (instance & 0x3FFFFFu);
}

void splitObjectId(uint32_t objectId,
                   uint16_t &objectType,
                   uint32_t &instance)
{
    objectType = static_cast<uint16_t>((objectId >> 22) & 0x3FFu);
    instance = objectId & 0x3FFFFFu;
}

} // namespace bacnet
