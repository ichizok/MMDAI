/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2010-2013  hkrn                                    */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAI project team nor the names of     */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

#pragma once
#ifndef VPVL2_EXTENSIONS_ICU_STRING_H_
#define VPVL2_EXTENSIONS_ICU_STRING_H_

#include <string>
#include <vpvl2/IString.h>

/* ICU */
#include <unicode/unistr.h>
#include <unicode/ucnv.h>

namespace vpvl2
{
namespace extensions
{
namespace icu4c
{

class String : public IString {
public:
    struct Converter {
        Converter()
            : shiftJIS(0),
              utf8(0),
              utf16(0)
        {
        }
        ~Converter() {
            ucnv_close(utf8);
            ucnv_close(utf16);
            ucnv_close(shiftJIS);
        }
        void open() {
            UErrorCode status = U_ZERO_ERROR;
            utf8  = ucnv_open("utf-8", &status);
            utf16 = ucnv_open("utf-16le", &status);
            shiftJIS  = ucnv_open("shift_jis", &status);
        }
        UConverter *shiftJIS;
        UConverter *utf8;
        UConverter *utf16;
    };

    static inline const std::string toStdString(const UnicodeString &value) {
        std::string str;
        UErrorCode status = U_ZERO_ERROR;
        size_t length = value.extract(0, 0, static_cast<UConverter *>(0), status);
        str.resize(length + 1);
        status = U_ZERO_ERROR;
        value.extract(&str[0], str.size(), 0, status);
        return str;
    }
    static inline bool toBoolean(const UnicodeString &value) {
        return value == "true" || value == "1" || value == "y" || value == "yes";
    }
    static inline int toInt(const UnicodeString &value, int def = 0) {
        int v = int(strtol(toStdString(value).c_str(), 0, 10));
        return v != 0 ? v : def;
    }
    static inline double toDouble(const UnicodeString &value, double def = 0.0) {
        double v = strtod(toStdString(value).c_str(), 0);
        return v != 0.0 ? float(v) : def;
    }

    explicit String(const UnicodeString &value, const Converter *converterRef = 0)
        : m_converterRef(converterRef),
          m_value(value)
    {
        UErrorCode status = U_ZERO_ERROR;
        size_t length = value.extract(0, 0, static_cast<UConverter *>(0), status);
        status = U_ZERO_ERROR;
        m_bytes.resize(length + 1);
        value.extract(reinterpret_cast<char *>(&m_bytes[0]),
                      m_bytes.count(),
                      static_cast<UConverter *>(0), status);
    }
    ~String() {
        m_converterRef = 0;
    }

    bool startsWith(const IString *value) const {
        return m_value.startsWith(static_cast<const String *>(value)->value()) == TRUE;
    }
    bool contains(const IString *value) const {
        return m_value.indexOf(static_cast<const String *>(value)->value()) != -1;
    }
    bool endsWith(const IString *value) const {
        return m_value.endsWith(static_cast<const String *>(value)->value()) == TRUE;
    }
    void split(const IString *separator, int maxTokens, Array<IString *> &tokens) const {
        tokens.clear();
        if (maxTokens > 0) {
            const UnicodeString &sep = static_cast<const String *>(separator)->value();
            int32_t offset = 0, pos = 0, size = sep.length(), nwords = 0;
            while ((pos = m_value.indexOf(sep, offset)) >= 0) {
                tokens.append(new String(m_value.tempSubString(offset, pos - offset), m_converterRef));
                offset = pos + size;
                nwords++;
                if (nwords >= maxTokens) {
                    break;
                }
            }
            if (maxTokens - nwords == 0) {
                int lastArrayOffset = tokens.count() - 1;
                IString *s = tokens[lastArrayOffset];
                const UnicodeString &s2 = static_cast<const String *>(s)->value();
                const UnicodeString &sp = static_cast<const String *>(separator)->value();
                tokens[lastArrayOffset] = new String(s2 + sp + m_value.tempSubString(offset), m_converterRef);
                delete s;
            }
        }
        else if (maxTokens == 0) {
            tokens.append(new String(m_value, m_converterRef));
        }
        else {
            const UnicodeString &sep = static_cast<const String *>(separator)->value();
            int32_t offset = 0, pos = 0, size = sep.length();
            while ((pos = m_value.indexOf(sep, offset)) >= 0) {
                tokens.append(new String(m_value.tempSubString(offset, pos - offset), m_converterRef));
                offset = pos + size;
            }
            tokens.append(new String(m_value.tempSubString(offset), m_converterRef));
        }
    }
    IString *clone() const {
        return new String(m_value, m_converterRef);
    }
    const HashString toHashString() const {
        return HashString(reinterpret_cast<const char *>(&m_bytes[0]));
    }
    bool equals(const IString *value) const {
        return (m_value == static_cast<const String *>(value)->value());
    }
    UnicodeString value() const {
        return m_value;
    }
    const uint8_t *toByteArray() const {
        return &m_bytes[0];
    }
    size_t size() const {
        return m_value.length();
    }
    size_t length(Codec codec) const {
        if (m_converterRef) {
            UErrorCode status = U_ZERO_ERROR;
            UConverter *converter = 0;
            switch (codec) {
            case kShiftJIS:
                converter = m_converterRef->shiftJIS;
                break;
            case kUTF8:
                converter = m_converterRef->utf8;
                break;
            case kUTF16:
                converter = m_converterRef->utf16;
                break;
            case kMaxCodecType:
            default:
                break;
            }
            return m_value.extract(0, 0, converter, status);
        }
        else {
            const char *codecString = "utf-8";
            switch (codec) {
            case kShiftJIS:
                codecString = "shift_jis";
                break;
            case kUTF8:
                codecString = "utf-8";
                break;
            case kUTF16:
                codecString = "utf-16le";
                break;
            case kMaxCodecType:
            default:
                break;
            }
            return m_value.extract(0, m_value.length(), 0, codecString);
        }
    }

private:
    const Converter *m_converterRef;
    const UnicodeString m_value;
    Array<uint8_t> m_bytes;

    VPVL2_DISABLE_COPY_AND_ASSIGN(String)
};

} /* namespace icu */
} /* namespace extensions */
} /* namespace vpvl2 */

#endif