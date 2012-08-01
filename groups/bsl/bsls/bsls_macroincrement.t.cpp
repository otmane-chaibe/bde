// bsls_macroincrement.t.cpp                                          -*-C++-*-

#include <bsls_macroincrement.h>

#include <cstdlib>     // atoi()
#include <iostream>

// using namespace BloombergLP;
using namespace std;

//=============================================================================
//                                 TEST PLAN
//-----------------------------------------------------------------------------
//                                  Overview
//                                  --------
// The component under test defines a macro that increments given argument in
// preprocessors.  There is no primary manipulator or basic accessor of this
// component.
//-----------------------------------------------------------------------------
// [ 1] BSLS_MACROINCREMENT(MACRO_VALUE)
//-----------------------------------------------------------------------------
// [ 2] USAGE EXAMPLE
//=============================================================================
//                       STANDARD BDE ASSERT TEST MACRO
//-----------------------------------------------------------------------------
static int testStatus = 0;

static void aSsErT(int c, const char *s, int i)
{
    if (c) {
        cout << "Error " << __FILE__ << "(" << i << "): " << s
             << "    (failed)" << endl;
        if (testStatus >= 0 && testStatus <= 100) ++testStatus;
    }
}

#define ASSERT(X) { aSsErT(!(X), #X, __LINE__); }

//=============================================================================
//                    STANDARD BDE LOOP-ASSERT TEST MACROS
//-----------------------------------------------------------------------------
#define LOOP_ASSERT(I,X) { \
   if (!(X)) { cout << #I << ": " << I << "\n"; aSsErT(1, #X, __LINE__); }}

//=============================================================================
//                      SEMI-STANDARD TEST OUTPUT MACROS
//-----------------------------------------------------------------------------
#define P(X) cout << #X " = " << (X) << endl; // Print identifier and value.
#define Q(X) cout << "<| " #X " |>" << endl;  // Quote identifier literally.
#define P_(X) cout << #X " = " << (X) << ", "<< flush; // P(X) without '\n'
#define TAB cout << '\t';                     // output the tab character.
#define L_ __LINE__                           // current Line number

//==========================================================================
//                    GLOBAL HELPER FUNCTIONS FOR TESTING
//--------------------------------------------------------------------------

#define LINENO(x)  x

//=============================================================================
//                                MAIN PROGRAM
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    int test = argc > 1 ? atoi(argv[1]) : 0;
    int verbose = argc > 2;
    int veryVerbose = argc > 3;

    cout << "TEST " << __FILE__ << " CASE " << test << endl;

    switch (test) { case 0:
      case 2: {
        // --------------------------------------------------------------------
        // USAGE EXAMPLE
        //   Simple example illustrating how one might use the macro defined
        //   in this component.
        //
        // Concerns:
        //:  The usage example provided in the component header file must
        //:  compile, link, and run on all platforms as shown.
        //
        // Plan:
        //:  Incorporate usage example from header into driver, remove leading
        //:  comment characters, and replace 'assert' with 'ASSERT'.
        //
        // Testing:
        //   USAGE EXAMPLE
        // --------------------------------------------------------------------

        if (verbose) cout << endl << "USAGE EXAMPLE" << endl
                                  << "=============" << endl;

        cout << "__LINE__, __FILE__: " << __LINE__ << ", " __FILE__ << "\n";
        #line BSLS_MACROINCREMENT(__LINE__) "some_other_filename.cpp"
        cout << "__LINE__, __FILE__: " << __LINE__ << ", " __FILE__ << "\n";
      } break;
      case 1: {
        // --------------------------------------------------------------------
        // TEST BSLS_MACROINCREMENT
        //   This test verifies correctness of 'BSLS_MACROINCREMENT' macro.
        //
        // Concerns:
        //:  1 The 'BSLS_MACROINCREMENT' correctly increments the given
        //:    argument (either an integer literal or a macro that ultimately
        //:    expands to an integer literal) in preprocessors.
        //
        // Plan:
        //:  1 For integer literal V in the range '[0, 19999]', set
        //:    'BSLS_MACROINCREMENT(V)' as the first argument following '#line'
        //:    directive.  Verify the line number is correctly incremented.
        //:    (C-1)
        //:
        //:  2 For integer literal V in the range '[0, 19999]', set
        //:    'BSLS_MACROINCREMENT(LINENO(V))' as the first argument following
        //:    '#line' directive where 'LINENO' is a macro eventually expanding
        //:     to an integer liternal in range '[0, 19999']'.  Verify the line
        //:     number is correctly incremented.  (C-1)
        //
        // Testing:
        //   BSLS_MACROINCREMENT
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "Testing 'BSLS_MACROINCREMENT' on integer literal"
                          << endl
                          << "================================================"
                          << endl;
        {
#line BSLS_MACROINCREMENT(0)
            if (veryVerbose) { P(__LINE__); } ASSERT(    1 == __LINE__);
#line BSLS_MACROINCREMENT(1)
            if (veryVerbose) { P(__LINE__); } ASSERT(    2 == __LINE__);
#line BSLS_MACROINCREMENT(2)
            if (veryVerbose) { P(__LINE__); } ASSERT(    3 == __LINE__);
#line BSLS_MACROINCREMENT(3)
            if (veryVerbose) { P(__LINE__); } ASSERT(    4 == __LINE__);
#line BSLS_MACROINCREMENT(4)
            if (veryVerbose) { P(__LINE__); } ASSERT(    5 == __LINE__);
#line BSLS_MACROINCREMENT(5)
            if (veryVerbose) { P(__LINE__); } ASSERT(    6 == __LINE__);
#line BSLS_MACROINCREMENT(100)
            if (veryVerbose) { P(__LINE__); } ASSERT(  101 == __LINE__);
#line BSLS_MACROINCREMENT(1000)
            if (veryVerbose) { P(__LINE__); } ASSERT( 1001 == __LINE__);
#line BSLS_MACROINCREMENT(5769)
            if (veryVerbose) { P(__LINE__); } ASSERT( 5770 == __LINE__);
#line BSLS_MACROINCREMENT(10000)
            if (veryVerbose) { P(__LINE__); } ASSERT(10001 == __LINE__);
#line BSLS_MACROINCREMENT(12345)
            if (veryVerbose) { P(__LINE__); } ASSERT(12346 == __LINE__);
#line BSLS_MACROINCREMENT(19999)
            if (veryVerbose) { P(__LINE__); } ASSERT(20000 == __LINE__);
        }

        if (verbose) cout << endl
                          << "Testing 'BSLS_MACROINCREMENT' on macro" << endl
                          << "======================================" << endl;
        {
#line BSLS_MACROINCREMENT(LINENO(0))
            if (veryVerbose) { P(__LINE__); } ASSERT(    1 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(1))
            if (veryVerbose) { P(__LINE__); } ASSERT(    2 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(2))
            if (veryVerbose) { P(__LINE__); } ASSERT(    3 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(3))
            if (veryVerbose) { P(__LINE__); } ASSERT(    4 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(4))
            if (veryVerbose) { P(__LINE__); } ASSERT(    5 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(5))
            if (veryVerbose) { P(__LINE__); } ASSERT(    6 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(100))
            if (veryVerbose) { P(__LINE__); } ASSERT(  101 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(1000))
            if (veryVerbose) { P(__LINE__); } ASSERT( 1001 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(5769))
            if (veryVerbose) { P(__LINE__); } ASSERT( 5770 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(10000))
            if (veryVerbose) { P(__LINE__); } ASSERT(10001 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(12345))
            if (veryVerbose) { P(__LINE__); } ASSERT(12346 == __LINE__);
#line BSLS_MACROINCREMENT(LINENO(19999))
            if (veryVerbose) { P(__LINE__); } ASSERT(20000 == __LINE__);
        }
      } break;
      default: {
        cerr << "WARNING: CASE `" << test << "' NOT FOUND." << endl;
        testStatus = -1;
      }
    }

    if (testStatus > 0) {
        cerr << "Error, non-zero test status = " << testStatus << "." << endl;
    }
    return testStatus;
}

// ---------------------------------------------------------------------------
// NOTICE:
//      Copyright (C) Bloomberg L.P., 2010
//      All Rights Reserved.
//      Property of Bloomberg L.P. (BLP)
//      This software is made available solely pursuant to the
//      terms of a BLP license agreement which governs its use.
// ----------------------------- END-OF-FILE ---------------------------------
