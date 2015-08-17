// bdlqq_configuration.cpp                                            -*-C++-*-
#include <bdlqq_configuration.h>

#include <bdlqq_threadattributes.h>

#include <bsls_atomicoperations.h>
#include <bdlqq_platform.h>

#include <bslmf_assert.h>
#include <bsls_assert.h>
#include <bsls_platform.h>
#include <bsls_types.h>

#include <bsl_cstddef.h>
#include <bsl_c_limits.h>

#if defined(BDLQQ_PLATFORM_POSIX_THREADS)
# include <pthread.h>
#elif defined(BDLQQ_PLATFORM_WIN32_THREADS)
# include <windows.h>
#else
# error unrecognized threading platform
#endif

#include <bsls_ident.h>
BSLS_IDENT_RCSID(bdlqq_configuration_cpp,"$Id$ $CSID$")

#if defined(BDLQQ_PLATFORM_POSIX_THREADS)

# ifdef BSLS_PLATFORM_OS_SOLARIS
static int nativeDefaultThreadStackSizeImp()
    // Return the native thread stack size for Solaris.
{
    // 1 megabyte on 32 bit, 2 megabytes on 64 bit

    enum { SOLARIS_DEFAULT_STACK_SIZE = 256 * 1024 * sizeof(void *) };
    return SOLARIS_DEFAULT_STACK_SIZE;
}
# else // POSIX_THREADS && !Solaris
static int nativeDefaultThreadStackSizeImp()
    // Return the native thread stack size for pthreads platforms other than
    // Solaris.
{
    bsl::size_t threadStackSize;

    {
        pthread_attr_t attr;

        int rc = pthread_attr_init(&attr);
        BSLS_ASSERT(0 == rc);
        rc = pthread_attr_getstacksize(&attr, &threadStackSize);
        BSLS_ASSERT(0 == rc);
        rc = pthread_attr_destroy(&attr);
        BSLS_ASSERT(0 == rc);
    }

    BSLS_ASSERT(1 <= threadStackSize);
    BSLS_ASSERT(threadStackSize <= static_cast<bsl::size_t>(INT_MAX));

#  ifdef PTHREAD_STACK_MIN
    BSLS_ASSERT(threadStackSize >= PTHREAD_STACK_MIN);
#  endif

#  ifdef BSLS_PLATFORM_OS_HPUX
    // On HPUX (Itanium) there is an additional stack for register storage,
    // effectively halving the amount of memory available to clients, so we
    // adjust the returned value to reflect the amount of memory that will be
    // available to clients for initializing data on the stack.  Note that
    // mechanisms for creating a thread should have a corresponding inverse of
    // this adjustment (see 'bdlqq_threadutil').

    threadStackSize /= 2;
#  endif

    return static_cast<int>(threadStackSize);
}
# endif
#else // WIN32_THREADS
static int nativeDefaultThreadStackSizeImp()
    // Return the native thread stack size for Windows.
{
    enum { WINDOWS_DEFAULT_STACK_SIZE = 0x100000 };    // 1 megabyte

    bsl::size_t threadStackSize = WINDOWS_DEFAULT_STACK_SIZE;

    // obtain default stack reserve size from the PE header

    char *imageBase = (char *) GetModuleHandle(NULL);
    if (imageBase) {
        IMAGE_OPTIONAL_HEADER *header =
             (IMAGE_OPTIONAL_HEADER *) (imageBase
                   + ((IMAGE_DOS_HEADER *) imageBase)->e_lfanew
                   + sizeof (IMAGE_NT_SIGNATURE) + sizeof (IMAGE_FILE_HEADER));
        threadStackSize = header->SizeOfStackReserve;
    }

    BSLS_ASSERT(1 <= threadStackSize);
    BSLS_ASSERT(threadStackSize <= static_cast<bsl::size_t>(INT_MAX));

    return static_cast<int>(threadStackSize);
}
#endif


namespace BloombergLP {

static bsls::AtomicOperations::AtomicTypes::Int
defaultThreadStackSizeValue = { -1 };

int bdlqq::Configuration::defaultThreadStackSize()
{
    if (bsls::AtomicOperations::getIntRelaxed(&defaultThreadStackSizeValue) <
                                                                           0) {
        return ThreadAttributes::e_UNSET_STACK_SIZE;              // RETURN
    }

    return bsls::AtomicOperations::getIntRelaxed(&defaultThreadStackSizeValue);
}

int bdlqq::Configuration::nativeDefaultThreadStackSize()
{
    static bsls::AtomicOperations::AtomicTypes::Int ret = { -1 };

    if (bsls::AtomicOperations::getIntRelaxed(&ret) < 0) {
        bsls::AtomicOperations::setIntRelaxed(&ret,
                                       nativeDefaultThreadStackSizeImp());
        BSLS_ASSERT(bsls::AtomicOperations::getIntRelaxed(&ret) >= 1);
    }

    return bsls::AtomicOperations::getIntRelaxed(&ret);
}

int bdlqq::Configuration::nativeDefaultThreadGuardSize()
{
#if defined(BDLQQ_PLATFORM_POSIX_THREADS)
    static bsls::AtomicOperations::AtomicTypes::Int ret = { -1 };

    if (bsls::AtomicOperations::getIntRelaxed(&ret) < 0) {
        pthread_attr_t attr;
        int rc = pthread_attr_init(&attr);
        BSLS_ASSERT(0 == rc);

        bsl::size_t guardSizeT;
        rc = pthread_attr_getguardsize(&attr, &guardSizeT);
        BSLS_ASSERT(0 == rc);

        rc = pthread_attr_destroy(&attr);
        BSLS_ASSERT(0 == rc);

        BSLS_ASSERT(guardSizeT > 0);
        BSLS_ASSERT(guardSizeT <= static_cast<bsl::size_t>(INT_MAX));

        bsls::AtomicOperations::setIntRelaxed(&ret,
                                              static_cast<int>(guardSizeT));
    }

    return bsls::AtomicOperations::getIntRelaxed(&ret);
#else
    // WIN32_THREADS

    return 0;    // guard size is ignored on Windows
#endif
}

int bdlqq::Configuration::recommendedDefaultThreadStackSize()
{
    // 1 megabyte on 32 bit, 2 megabytes on 64 bit, constant across platforms

    enum { RECOMMENDED_DEFAULT_STACKSIZE = 256 * 1024 * sizeof(void *) };

#ifdef PTHREAD_STACK_MIN
    // Note -- this cannot be a BSLMF_ASSERT -- 'PTHREAD_STACK_MIN' is a
    // function call on some platforms.

    BSLS_ASSERT(RECOMMENDED_DEFAULT_STACKSIZE >= PTHREAD_STACK_MIN);
#endif

    return RECOMMENDED_DEFAULT_STACKSIZE;
}

void bdlqq::Configuration::setDefaultThreadStackSize(int numBytes)
{
#if defined(BDLQQ_PLATFORM_POSIX_THREADS) && defined(PTHREAD_STACK_MIN)
    BSLS_ASSERT_OPT(numBytes >= static_cast<int>(PTHREAD_STACK_MIN));
#else
    BSLS_ASSERT_OPT(numBytes > 0);
#endif

    bsls::AtomicOperations::setIntRelaxed(&defaultThreadStackSizeValue,
                                          numBytes);
}

}  // close enterprise namespace

// ----------------------------------------------------------------------------
// Copyright 2015 Bloomberg Finance L.P.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------- END-OF-FILE ----------------------------------
