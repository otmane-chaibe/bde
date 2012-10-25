// baejsn_parserutil.h                                                -*-C++-*-
#ifndef INCLUDED_BAEJSN_PARSERUTIL
#define INCLUDED_BAEJSN_PARSERUTIL

#ifndef INCLUDED_BDES_IDENT
#include <bdes_ident.h>
#endif
BDES_IDENT("$Id: $")

//@PURPOSE: Provide functions to encode and decode simple types in JSON format.
//
//@CLASSES:
//
//@SEE_ALSO:
//
//@AUTHOR: Raymond Chiu (schiu49)
//
//@DESCRIPTION: This component provides utility functions for encoding and
// decoding of primitive JSON constructs fundamental C++ types.

#ifndef INCLUDED_BDESCM_VERSION
#include <bdescm_version.h>
#endif

#ifndef INCLUDED_BDEPU_ISO8601
#include <bdepu_iso8601.h>
#endif

#ifndef INCLUDED_BSL_STREAMBUF
#include <bsl_streambuf.h>
#endif

#ifndef INCLUDED_BSLS_TYPES
#include <bsls_types.h>
#endif

#ifndef INCLUDED_BSL_CLIMITS
#include <bsl_climits.h>
#endif

namespace BloombergLP {


                            // ========================
                            // struct baejsn_ParserUtil
                            // ========================

struct baejsn_ParserUtil {
  private:

    template <typename TYPE>
    static int getDateAndTimeValue(bsl::streambuf *streamBuf,
                                   TYPE           *value,
                                   int             maxLength);

    template <typename TYPE>
    static int getNumericalValue(bsl::streambuf *streamBuf, TYPE *value);

    static int getUint64(bsl::streambuf      *streamBuf,
                         bsls::Types::Uint64 *value);

    static int getValueImp(bsl::streambuf *streamBuf, bool            *value);
    static int getValueImp(bsl::streambuf *streamBuf, char            *value);
    static int getValueImp(bsl::streambuf *streamBuf, unsigned char   *value);
    static int getValueImp(bsl::streambuf *streamBuf, signed char     *value);
    static int getValueImp(bsl::streambuf *streamBuf, short           *value);
    static int getValueImp(bsl::streambuf *streamBuf, unsigned short  *value);
    static int getValueImp(bsl::streambuf *streamBuf, int             *value);
    static int getValueImp(bsl::streambuf *streamBuf, unsigned int    *value);
    static int getValueImp(bsl::streambuf     *streamBuf,
                           bsls::Types::Int64 *value);
    static int getValueImp(bsl::streambuf      *streamBuf,
                           bsls::Types::Uint64 *value);
    static int getValueImp(bsl::streambuf *streamBuf, float           *value);
    static int getValueImp(bsl::streambuf *streamBuf, double          *value);
    static int getValueImp(bsl::streambuf *streamBuf, bsl::string     *value);
    static int getValueImp(bsl::streambuf *streamBuf, bdet_Date       *value);
    static int getValueImp(bsl::streambuf *streamBuf, bdet_Datetime   *value);
    static int getValueImp(bsl::streambuf *streamBuf, bdet_DatetimeTz *value);
    static int getValueImp(bsl::streambuf *streamBuf, bdet_DateTz     *value);
    static int getValueImp(bsl::streambuf *streamBuf, bdet_Time       *value);
    static int getValueImp(bsl::streambuf *streamBuf, bdet_TimeTz     *value);

    static int getInteger(bsl::streambuf      *streamBuf,
                          bsls::Types::Int64  *value);

    static int getDouble(bsl::streambuf *streamBuf,
                         double         *value);

  public:
    static int skipSpaces(bsl::streambuf *streamBuf);

    static int getString(bsl::streambuf *streamBuf, bsl::string *value);

    static int eatToken(bsl::streambuf *streamBuf, const char *token);
        // If the get pointer of specified 'streamBuf' refers to a string that
        // matches the specified 'token', advance the get pointer to the
        // charater just after the matched string, with no effect otherwise.
        // Return 0 if a match is found, and a non-zero value otherwise.

    static int advancePastWhitespaceAndToken(bsl::streambuf *streamBuf,
                                             char            token);
        // TBD

    static int getInteger(bsl::streambuf      *streamBuf,
                          bsls::Types::Uint64 *value);
        // TBD make private

    template <class TYPE>
    static int getNumber(bsl::streambuf *streamBuf, TYPE *value);
        // TBD make private

    template <typename TYPE>
    static int putValue(bsl::streambuf *streamBuf, const TYPE& value);

    template <typename TYPE>
    static int getValue(bsl::streambuf *streamBuf, TYPE *value);
};

// ============================================================================
//                      INLINE FUNCTION DEFINITIONS
// ============================================================================

                            // ------------------------
                            // struct baejsn_ParserUtil
                            // ------------------------

// PRIVATE METHODS
template <typename TYPE>
int baejsn_ParserUtil::getDateAndTimeValue(bsl::streambuf *streamBuf,
                                           TYPE           *value,
                                           int             maxLength)
{
    bsl::string temp;
    if (0 != getString(streamBuf, &temp)) {
        return -1;                                                    // RETURN
    }

    return temp.length() <= static_cast<unsigned int>(maxLength)
         ? bdepu_Iso8601::parse(value, temp.data(), temp.length())
         : -1;
}

template <typename TYPE>
inline
int baejsn_ParserUtil::getNumericalValue(bsl::streambuf *streamBuf,
                                         TYPE           *value)
{
    double temp = 0;
    int rc = getDouble(streamBuf, &temp);

    const double tolerance = 0.99;
    const double low = temp + tolerance;  // accepts -TYPE_MIN.99
    const double hi  = temp - tolerance;  // accepts  TYPE_MAX.99

    if (low < static_cast<double>(bsl::numeric_limits<TYPE>::min())
     || hi > static_cast<double>(bsl::numeric_limits<TYPE>::max())) {
        return -1;
    }

    *value = static_cast<TYPE>(temp);
    return rc;
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf, bool *value)
{
    if (0 == eatToken(streamBuf, "true")) {
        *value = true;
    }
    else if (0 == eatToken(streamBuf, "false")) {
        *value = false;
    }
    else {
        return -1;                                                    // RETURN
    }
    return 0;
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   char           *value)
{
    bsl::string valueString;

    if (0 == getString(streamBuf, &valueString)
     && 1 == valueString.length()) {
        *value = valueString[0];
    }
    else {
        return -1;                                                    // RETURN
    }
    return 0;
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   unsigned char  *value)
{
    return getNumericalValue(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   signed char    *value)
{
    return getValueImp(streamBuf, (char *) value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf, short *value)
{
    return getNumericalValue(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   unsigned short *value)
{
    return getNumericalValue(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf, int *value)
{
    return getNumericalValue(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   unsigned int   *value)
{
    return getNumericalValue(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf     *streamBuf,
                                   bsls::Types::Int64 *value)
{
    int ch = streamBuf->sgetc();

    bool isNegative;
    if ('-' == ch) {
        isNegative = true;
        streamBuf->snextc();
    }
    else {
        isNegative = false;
    }

    bsls::Types::Uint64 tmp = 0;
    if (0 != getUint64(streamBuf, &tmp)) {
        return -1;                                                    // RETURN
    }

    bsls::Types::Int64 tmp2 = static_cast<bsls::Types::Int64>(tmp);

    if (tmp2 < bsl::numeric_limits<bsls::Types::Int64>::min()
     || tmp2 > bsl::numeric_limits<bsls::Types::Int64>::max()) {
        return -1;                                                    // RETURN
    }

    if (isNegative) {
        *value = tmp2 * -1;
    }
    else {
        *value = tmp2;
    }

    return 0;
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf      *streamBuf,
                                   bsls::Types::Uint64 *value)
{
    bsls::Types::Uint64 tmp = 0;
    const int rc = getUint64(streamBuf, &tmp);
    if (!rc) {
        *value = tmp;
    }
    return rc;
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   float          *value)
{
    double temp;
    const int rc = getDouble(streamBuf, &temp);
    if (!rc) {
        *value = static_cast<float>(temp);
    }
    return rc;
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   double         *value)
{
    return getDouble(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   bsl::string    *value)
{
    return getString(streamBuf, value);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   bdet_Date      *value)
{
    return getDateAndTimeValue(streamBuf,
                               value,
                               bdepu_Iso8601::BDEPU_DATE_STRLEN);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   bdet_Datetime  *value)
{
    return getDateAndTimeValue(streamBuf,
                               value,
                               bdepu_Iso8601::BDEPU_DATETIME_STRLEN);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf  *streamBuf,
                                   bdet_DatetimeTz *value)
{
    return getDateAndTimeValue(streamBuf,
                               value,
                               bdepu_Iso8601::BDEPU_DATETIMETZ_STRLEN);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   bdet_DateTz    *value)
{
    return getDateAndTimeValue(streamBuf,
                               value,
                               bdepu_Iso8601::BDEPU_DATETZ_STRLEN);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   bdet_Time      *value)
{
    return getDateAndTimeValue(streamBuf,
                               value,
                               bdepu_Iso8601::BDEPU_TIME_STRLEN);
}

inline
int baejsn_ParserUtil::getValueImp(bsl::streambuf *streamBuf,
                                   bdet_TimeTz    *value)
{
    return getDateAndTimeValue(streamBuf,
                               value,
                               bdepu_Iso8601::BDEPU_TIMETZ_STRLEN);
}

template <class TYPE>
int baejsn_ParserUtil::getNumber(bsl::streambuf *streamBuf,
                                 TYPE           *value)
{
    double temp;
    const int rc = getDouble(streamBuf, &temp);
    if (!rc) {
        *value = static_cast<TYPE>(temp);
    }
    return rc;
}

template <class TYPE>
inline
int baejsn_ParserUtil::getValue(bsl::streambuf *streamBuf, TYPE *value)
{
    return getValueImp(streamBuf, value);
}

}  // close namespace BloombergLP

#endif

// ---------------------------------------------------------------------------
// NOTICE:
//      Copyright (C) Bloomberg L.P., 2012
//      All Rights Reserved.
//      Property of Bloomberg L.P. (BLP)
//      This software is made available solely pursuant to the
//      terms of a BLP license agreement which governs its use.
// ----------------------------- END-OF-FILE ---------------------------------
