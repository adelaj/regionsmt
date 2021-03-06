#include "ll.h"
#include "utf8.h"

uint8_t utf8_len(uint32_t utf8_val)
{
    uint8_t mode = (uint8_t) bsr(utf8_val);
    if (!utf8_val || mode <= 6) return 1;
    return (mode + 4) / 5;
}

bool utf8_is_overlong(uint32_t utf8_val, uint8_t utf8_len)
{
    switch (utf8_len)
    {
    case 0:
    case 1:
        break;
    case 2:
        return bsr(utf8_val) <= 6;
    default:
        return bsr(utf8_val) <= 5 * ((uint32_t) utf8_len - 1);
    }
    return 0;
}

bool utf8_is_valid(uint32_t utf8_val, uint8_t utf8_len)
{
    return utf8_val <= UTF8_BOUND && !utf8_is_overlong(utf8_val, utf8_len);
}

bool utf8_is_whitespace(uint32_t utf8_val)
{
    return utf8_val == 0x20 || utf8_val == '\n' || utf8_val == '\r' || utf8_val == 0x9;
}

bool utf8_is_whitespace_len(uint32_t utf8_val, uint8_t utf8_len)
{
    return utf8_len == 1 && utf8_is_whitespace(utf8_val);
}

bool utf8_is_xml_char(uint32_t utf8_val)
{
    return utf8_val == 0x9 || utf8_val == '\n' || utf8_val == '\r' ||
        (0x20 <= utf8_val && utf8_val <= 0xd7ff) ||
        (0xe000 <= utf8_val && utf8_val <= 0xfffd) ||
        (0x10000 <= utf8_val && utf8_val <= 0x10ffff);
}

bool utf8_is_xml_char_len(uint32_t utf8_val, uint8_t utf8_len)
{
    switch (utf8_len)
    {
    case 1:
        return utf8_val == 0x9 || utf8_val == '\n' || utf8_val == '\r' || 0x20 <= utf8_val;
    case 2:
        return 1;
    case 3:
        return utf8_val <= 0xd7ff || (0xe000 <= utf8_val && utf8_val <= 0xfffd);
    case 4:
        return utf8_val <= 0x10ffff;
    }
    return 0;
}

bool utf8_is_xml_name_start_char_len(uint32_t utf8_val, uint8_t utf8_len)
{
    switch (utf8_len)
    {
    case 1:
        return utf8_val == ':' || ('A' <= utf8_val && utf8_val <= 'Z') || utf8_val == '_' || ('a' <= utf8_val && utf8_val <= 'z');
    case 2:
        return
            (0xc0 <= utf8_val && utf8_val <= 0xd6) ||
            (0xd8 <= utf8_val && utf8_val <= 0xf6) ||
            (0xf8 <= utf8_val && utf8_val <= 0x2ff) ||
            (0x370 <= utf8_val && utf8_val <= 0x37d) ||
            0x37f <= utf8_val;
    case 3:
        return
            utf8_val <= 0x1fff ||
            (0x200c <= utf8_val && utf8_val <= 0x200d) ||
            (0x2070 <= utf8_val && utf8_val <= 0x218f) ||
            (0x2c00 <= utf8_val && utf8_val <= 0x2fef) ||
            (0x3001 <= utf8_val && utf8_val <= 0xd7ff) ||
            (0xf900 <= utf8_val && utf8_val <= 0xfdcf) ||
            (0xfdf0 <= utf8_val && utf8_val <= 0xfffd);
    case 4:
        return utf8_val <= 0xeffff;
    }
    return 0;
}

bool utf8_is_xml_name_char_len(uint32_t utf8_val, uint8_t utf8_len)
{
    if (utf8_is_xml_name_start_char_len(utf8_val, utf8_len)) return 1;
    switch (utf8_len)
    {
    case 1:
        return utf8_val == '-' || utf8_val == '.' || ('0' <= utf8_val && utf8_val <= '9');
    case 2:
        return utf8_val == 0xb7 || (0x300 <= utf8_val && utf8_val <= 0x36f);
    case 3:
        return (0x203f <= utf8_val && utf8_val <= 0x2040);
    }
    return 0;
}

void utf8_encode(uint32_t utf8_val, char *restrict utf8_ch, uint8_t *restrict p_utf8_len)
{
    uint8_t mode = (uint8_t) bsr(utf8_val);
    if (!utf8_val || mode <= 6)
    {
        utf8_ch[0] = (char) utf8_val;
        if (p_utf8_len) *p_utf8_len = 1;
    }
    else
    {
        uint8_t utf8_len = (mode + 4) / 5;
        if (p_utf8_len) *p_utf8_len = utf8_len;
        for (uint8_t i = utf8_len; i > 1; utf8_ch[--i] = (char) (128 | (utf8_val & 63)), utf8_val >>= 6);
        utf8_ch[0] = (char) ((((1u << (utf8_len + 1)) - 1) << (8 - utf8_len)) | utf8_val);
    }
}

// The 'coroutine' decodes multi-byte sequence supplied by separate characters 'byte'. 
// It should be called until '*p_context' becomes zero. Returns '0' on error. 
// Warning: '*p_context' should be initialized with zero before the initial call
// and should not be modified between sequential calls!
bool utf8_decode(char ch, uint32_t *restrict p_utf8_val, char *restrict utf8_ch, uint8_t *restrict p_utf8_len, uint8_t *restrict p_utf8_context)
{
    if (*p_utf8_context)
    {
        if (((uint8_t) ch & 0xc0) == 0x80) // UTF-8 continuation byte
        {
            *p_utf8_val <<= 6;
            *p_utf8_val |= (uint8_t) ch & 0x3f;
            uint8_t utf8_context = (*p_utf8_context)--;
            if (utf8_ch && p_utf8_len) utf8_ch[*p_utf8_len - utf8_context] = ch;
            return 1;
        }
    }
    else
    {
        uint8_t mode = bsr((uint8_t) ~ch);
        switch (mode)
        {
        case UINT8_MAX: // Invalid UTF-8 bytes: '\xff'
        case 0: // and '\xfe'
        case 6: // Unexpected UTF-8 continuation byte
            break;
        case 7: // Single-byte UTF-8 character
            *p_utf8_context = 0;
            *p_utf8_val = (uint8_t) ch;
            if (p_utf8_len) *p_utf8_len = 1;
            if (utf8_ch) utf8_ch[0] = ch;
            return 1;
        default: // UTF-8 start byte (cases 1, 2, 3, 4, and 5)
            *p_utf8_context = 6 - mode;
            *p_utf8_val = (uint8_t) ch & ((1u << mode) - 1);
            if (p_utf8_len) *p_utf8_len = 7 - mode;
            if (utf8_ch) utf8_ch[0] = ch;
            return 1;
        }        
    }
    return 0;
}

bool utf8_decode_len(const char *restrict str, size_t len, size_t *restrict p_len)
{
    uint32_t val; // Never used
    uint8_t ulen = 0, context = 0;
    size_t ind = 0;
    for (; ind < len; ind++)
    {
        if (!utf8_decode(str[ind], &val, NULL, &ulen, &context)) break;
        if (context) continue;
        *p_len = ulen;
        return 1;
    }
    *p_len = ind;
    return 0;
}

uint8_t utf16_len(uint32_t utf16_val)
{
    return 1 + (utf16_val >= 0x10000);
}

void utf16_encode(uint32_t utf16_val, char16_t *restrict utf16_word, uint8_t *restrict p_utf16_len, bool be)
{
    if (utf16_val < 0x10000)
    {
        if (p_utf16_len) *p_utf16_len = 1;
        char16_t w0 = (char16_t) utf16_val;
        utf16_word[0] = be ? rol(w0, 8) : w0;
    }
    else
    {
        if (p_utf16_len) *p_utf16_len = 2;
        utf16_val -= 0x10000;
        char16_t w0 = (char16_t) (0xd800 | ((utf16_val >> 10) & 0x3ff)), w1 = (char16_t) (0xdc00 | (utf16_val & 0x3ff));
        if (be) utf16_word[0] = rol(w0, 8), utf16_word[1] = rol(w1, 8);
        else utf16_word[0] = w0, utf16_word[1] = w1;
    }
}

bool utf16_decode(char16_t word, uint32_t *restrict p_utf16_val, char16_t *restrict utf16_word, uint8_t *restrict p_utf16_len, uint8_t *restrict p_utf16_context, bool be)
{
    char16_t wordle = be ? rol(word, 8) : word;
    if (*p_utf16_context)
    {
        if (0xdc00 <= wordle && wordle <= 0xdfff)
        {
            uint32_t utf16_val = *p_utf16_val;
            utf16_val |= wordle & 0x3ff;
            utf16_val += 0x10000;
            *p_utf16_val = utf16_val;
            *p_utf16_context = 0;
            if (utf16_word) utf16_word[1] = word;
            return 1;
        }
    }
    else
    {
        if (wordle < 0xd800 || 0xdfff < wordle)
        {
            *p_utf16_context = 0;
            *p_utf16_val = wordle;
            if (p_utf16_len) *p_utf16_len = 1;
            if (utf16_word) utf16_word[0] = word;
            return 1;
        }
        else if (0xd800 <= wordle && wordle <= 0xdbff)
        {
            *p_utf16_context = 1;
            *p_utf16_val = (uint32_t) (wordle & 0x3ff) << 10;
            if (p_utf16_len) *p_utf16_len = 2;
            if (utf16_word) utf16_word[0] = word;
            return 1;
        }
    }
    return 0;
}
