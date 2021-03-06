/**

 Copyright (c) 2010-2014  hkrn

 All rights reserved.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
 - Neither the name of the MMDAI project team nor the names of
   its contributors may be used to endorse or promote products
   derived from this software without specific prior written
   permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

*/

#include <vpvl2/extensions/icu4c/Encoding.h>
#include <vpvl2/internal/util.h>

#include <cstring> /* for std::strlen */
#include <unicode/udata.h>

#if defined(VPVL2_OS_WINDOWS)
#define strncasecmp _strnicmp
#endif

namespace {

#include "ICUCommonData.inl"

}

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{
namespace extensions
{
namespace icu4c
{

const char *Encoding::commonDataPath()
{
    return "icudt52l.dat";
}

bool Encoding::initializeOnce()
{
    UErrorCode err = U_ZERO_ERROR;
    udata_setCommonData(g_icudt52l_dat, &err);
    return err == U_ZERO_ERROR;
}

Encoding::Encoding(const Dictionary *dictionaryRef)
    : m_dictionaryRef(dictionaryRef),
      m_null(UnicodeString(), IString::kUTF8, &m_converter),
      m_detector(0)
{
    UErrorCode status = U_ZERO_ERROR;
    m_detector = ucsdet_open(&status);
    m_converter.initialize();
}

Encoding::~Encoding()
{
    ucsdet_close(m_detector);
    m_detector = 0;
    m_dictionaryRef = 0;
}

const IString *Encoding::stringConstant(ConstantType value) const
{
    if (const IString *const *s = m_dictionaryRef->find(value)) {
        return *s;
    }
    return &m_null;
}

IString *Encoding::toString(const uint8 *value, vsize size, IString::Codec codec) const
{
    IString *s = 0;
    if (UConverter *converter = m_converter.converterFromCodec(codec)) {
        const char *str = reinterpret_cast<const char *>(value);
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString us(str, int(size), converter, status);
        /* remove head and trail spaces and 0x1a (appended by PMDEditor) */
        s = new (std::nothrow) String(us.trim().findAndReplace(UChar(0x1a), UChar()), codec, &m_converter);
    }
    return s;
}

IString *Encoding::toString(const uint8 *value, IString::Codec codec, vsize maxlen) const
{
    if (maxlen > 0 && value) {
        vsize size = std::strlen(reinterpret_cast<const char *>(value));
        return toString(value, btMin(maxlen, size), codec);
    }
    else {
        return new(std::nothrow) String(UnicodeString(), IString::kUTF8, &m_converter);
    }
}

vsize Encoding::estimateSize(const IString *value, IString::Codec codec) const
{
    if (const String *s = static_cast<const String *>(value)) {
        if (UConverter *converter = m_converter.converterFromCodec(codec)) {
            UErrorCode status = U_ZERO_ERROR;
            return s->value().extract(0, 0, converter, status);
        }
    }
    return 0;
}

uint8 *Encoding::toByteArray(const IString *value, IString::Codec codec, int &size) const
{
    uint8 *data = 0;
    if (value) {
        const String *s = static_cast<const String *>(value);
        if (UConverter *converter = m_converter.converterFromCodec(codec)) {
            const UnicodeString &src = s->value();
            UErrorCode status = U_ZERO_ERROR;
            int32_t newStringLength = src.extract(0, 0, converter, status) + 1;
            if (size >= 0) {
                size = newStringLength = std::min(newStringLength, size);
            }
            data = new (std::nothrow) uint8[newStringLength];
            if (data) {
                status = U_ZERO_ERROR;
                src.extract(reinterpret_cast<char *>(data), newStringLength, converter, status);
            }
        }
    }
    else {
        data = new (std::nothrow) uint8[1];
        if (data) {
            data[0] = 0;
        }
    }
    return data;
}

void Encoding::disposeByteArray(uint8 *&value) const
{
    internal::deleteObjectArray(value);
    value = 0;
}

IString::Codec Encoding::detectCodec(const char *data, vsize length) const
{
    UErrorCode status = U_ZERO_ERROR;
    ucsdet_setText(m_detector, data, int32_t(length), &status);
    const UCharsetMatch *const match = ucsdet_detect(m_detector, &status);
    const char *const charset = ucsdet_getName(match, &status);
    IString::Codec codec = IString::kUTF8;
    struct CodecMap {
        IString::Codec codec;
        const char *const name;
    };
    static const CodecMap codecMap[] = {
        { IString::kShiftJIS, "shift_jis" },
        { IString::kUTF8,     "utf-8"     },
        { IString::kUTF16,    "utf-16"    }
    };
    for (vsize i = 0; i < sizeof(codecMap) / sizeof(codecMap[0]); i++) {
        const CodecMap &item = codecMap[i];
        if (strncasecmp(charset, item.name, std::strlen(item.name)) == 0) {
            codec = item.codec;
        }
    }
    return codec;
}

IString *Encoding::createString(const UnicodeString &value) const
{
    return new String(value, IString::kUTF8, &m_converter);
}

} /* namespace icu4c */
} /* namespace extensions */
} /* namespace VPVL2_VERSION_NS */
} /* namespace vpvl2 */
