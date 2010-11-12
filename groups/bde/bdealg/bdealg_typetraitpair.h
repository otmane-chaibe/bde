// bdealg_typetraitpair.h   -*-C++-*-
#ifndef INCLUDED_BDEALG_TYPETRAITPAIR
#define INCLUDED_BDEALG_TYPETRAITPAIR

#ifndef INCLUDED_BDES_IDENT
#include <bdes_ident.h>
#endif
BDES_IDENT("$Id: $")

//@PURPOSE: Provide facilities for identifying 'bsl::pair'-like classes.
//
//@DEPRECATED: Use 'bslalg_typetraitpair' instead.
//
//@CLASSES:
//  bdealg_TypeTraitPair: for 'bsl::pair'-like classes
//
//@SEE_ALSO: bslmf_typetraits
//
//@AUTHOR: Herve Bronnimann (hbronnim)
//
//@DESCRIPTION:  This component provides a single traits class,
// 'bdealg_TypeTraitPair'.  A 'TYPE' that has this trait fulfills the following
// requirements, where 'mX' is a modifiable instance and 'X' a non-modifiable
// instance of 'TYPE':
//..
//  Valid expression     Type
//  ----------------     ----
//  TYPE::first_type
//  TYPE::second_type
//
//  mX.first             first_type
//  mX.second            second
//  X.first              const first_type
//  X.second             const second_type
//..
// Note that 'first' and 'second' are *not* member functions, but data members.

#ifndef INCLUDED_BDESCM_VERSION
#include <bdescm_version.h>
#endif

#ifndef INCLUDED_BSLALG_TYPETRAITPAIR
#include <bslalg_typetraitpair.h>
#endif

namespace BloombergLP {

                        // ===========================
                        // struct bdealg_TypeTraitPair
                        // ===========================

typedef bslalg_TypeTraitPair bdealg_TypeTraitPair;
    // A type, 'T', with this trait has two data members, 'first' and 'second'
    // of types 'T::first_type' and 'T::second_type', respectively.
    // Metafunctions can be used to probe and combine the traits of the
    // individual pair members.  For example, the pair is bitwise moveable
    // only if both 'first_type' and 'second_type' have the
    // 'bdealg_TypeTraitBitwiseMoveable' trait.  User-defined types will
    // rarely need this trait.

}  // close namespace BloombergLP

#endif

// ---------------------------------------------------------------------------
// NOTICE:
//      Copyright (C) Bloomberg L.P., 2008
//      All Rights Reserved.
//      Property of Bloomberg L.P. (BLP)
//      This software is made available solely pursuant to the
//      terms of a BLP license agreement which governs its use.
// ----------------------------- END-OF-FILE ---------------------------------
