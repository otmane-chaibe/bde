// bdlqq_threadutil.t.cpp                                             -*-C++-*-
#include <bdlqq_threadutil.h>

#include <bdlqq_configuration.h>
#include <bdlqq_threadattributes.h>
#include <bsls_atomic.h>
#include <bdlqq_platform.h>

#include <bdlf_bind.h>
#include <bsls_systemclocktype.h>
#include <bsls_systemtime.h>
#include <bdlt_currenttime.h>

#include <bslma_default.h>
#include <bslma_testallocator.h>
#include <bsls_assert.h>
#include <bsls_asserttest.h>
#include <bsls_platform.h>
#include <bsls_types.h>

#include <bsl_algorithm.h>
#include <bsl_cstdio.h>
#include <bsl_cstdlib.h>
#include <bsl_cstring.h>
#include <bsl_iomanip.h>
#include <bsl_iostream.h>
#include <bsl_map.h>
#include <bsl_set.h>

#include <errno.h>

#ifdef BDLQQ_PLATFORM_POSIX_THREADS
#include <pthread.h>

# ifdef BSLS_PLATFORM_OS_SOLARIS
#   include <sys/utsname.h>
# endif

#endif

#ifndef BSLS_PLATFORM_OS_WINDOWS
#include <alloca.h>
#endif

using namespace BloombergLP;
using namespace bsl;  // automatically added by script

//=============================================================================
//                  STANDARD BDE ASSERT TEST MACRO
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

//-----------------------------------------------------------------------------

#define LOOP_ASSERT(I,X) {                                                    \
    if (!(X)) { cout << #I << ": " << I << "\n"; aSsErT(1, #X, __LINE__); } }

#define LOOP2_ASSERT(I,J,X) {                                                 \
    if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": " << J << "\n";   \
                aSsErT(1, #X, __LINE__); } }
#define LOOP3_ASSERT(I,J,K,X) {                                               \
    if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": " << J <<         \
                        "\t" << #K << ": " << K << "\n";                      \
                aSsErT(1, #X, __LINE__); } }
#define LOOP4_ASSERT(I,J,K,L,X) {                                             \
    if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": " << J <<         \
                 "\t" << #K << ": " << K << "\t" << #L << ": " << L << "\n";  \
                aSsErT(1, #X, __LINE__); } }

//-----------------------------------------------------------------------------

#define P(X) cout << #X " = " << (X) << endl; // Print identifier and value.
#define Q(X) cout << "<| " #X " |>" << endl;  // Quote identifier literally.
#define P_(X) cout << #X " = " << (X) << ", " << flush; // P(X) without '\n'
#define L_ __LINE__                           // current Line number
#define T_() cout << '\t' << flush;           // Print tab w/o linefeed.

//=============================================================================
//                  GLOBAL TYPEDEFS/CONSTANTS FOR TESTING
//-----------------------------------------------------------------------------

typedef bdlqq::ThreadUtil       Obj;
typedef bdlqq::ThreadAttributes Attr;

int verbose;
int veryVerbose;
int veryVeryVerbose;

// ============================================================================
//                  NEGATIVE-TEST MACRO ABBREVIATIONS
// ----------------------------------------------------------------------------

#define ASSERT_FAIL(expr)      BSLS_ASSERTTEST_ASSERT_FAIL(expr)
#define ASSERT_PASS(expr)      BSLS_ASSERTTEST_ASSERT_PASS(expr)
#define ASSERT_FAIL_RAW(expr)  BSLS_ASSERTTEST_ASSERT_FAIL_RAW(expr)
#define ASSERT_PASS_RAW(expr)  BSLS_ASSERTTEST_ASSERT_PASS_RAW(expr)

#if !defined(BSLS_PLATFORM_OS_CYGWIN)
    const int MIN_GUARD_SIZE = 0;
#else
    const int MIN_GUARD_SIZE = 1;
#endif

//=============================================================================
//                  GLOBAL FUNCTIONS FOR TESTING
//-----------------------------------------------------------------------------

bsls::Types::IntPtr intPtrAbs(bsls::Types::IntPtr a)
{
    return a >= 0 ? a : -a;
}

bsl::ostream& operator<<(bsl::ostream&                            stream,
                         bdlqq::ThreadAttributes::SchedulingPolicy policy)
{
    switch (policy) {
#undef CASE
#define CASE(x) case bdlqq::ThreadAttributes::x: stream << #x; break
      CASE(e_SCHED_OTHER);
      CASE(e_SCHED_FIFO);
      CASE(e_SCHED_RR);
      CASE(e_SCHED_DEFAULT);
      default:  stream << "<UNKNOWN>";
#undef CASE
    }

    return stream;
}

//=============================================================================

namespace {
    // unnamed namespace for local resources

                            // ================
                            // class BigFunctor
                            // ================

class BigFunctor {
    // This 'class' is used in the allocator test.

    enum { k_BUFFER_SIZE = 20 << 10 };

    // DATA
    char buffer[k_BUFFER_SIZE];

  public:
    void operator()()
    {
        for (char *pc = buffer; pc < buffer + k_BUFFER_SIZE; ++pc) {
            *pc = 0x3f;
        }
    }
};

                        // ===================
                        // class ThreadChecker
                        // ===================

class ThreadChecker {

    bsls::AtomicInt d_count;

  public:
    // CREATORS
    ThreadChecker() : d_count(0) {}

    // MANIPULATORS
    void eval()
    {
       ++d_count;
    }

    bdlf::Function<void(*)()> getFunctor()
    {
       return bdlf::BindUtil::bind(&ThreadChecker::eval, this);
    }

    // ACCESSORS
    int count() const
    {
       return d_count;
    }
};

extern "C" void *myThreadFunction(void *)
    // Print to standard output "Another second has passed"
    // every second for five seconds.
{
    for (int i = 0; i < 3; ++i) {
        bdlqq::ThreadUtil::microSleep(0, 1);
        if (verbose) bsl::cout << "Another second has passed\n";
    }

    return 0;
}

extern "C" void *mySmallStackThreadFunction(void *threadArg)
    // Initialize a small object on the stack and do some work.
{
    char *initValue = (char *)threadArg;
    char Small[8];
    memset(&Small[0], *initValue, 8);
    // do some work ...
    return 0;
}

}  // close unnamed namespace

static
void createSmallStackSizeThread()
    // Create a detached thread with the small stack size
    // and perform some work
{
    enum { STACK_SIZE = 16384 };
    bdlqq::ThreadAttributes attributes;
    attributes.setDetachedState(
                               bdlqq::ThreadAttributes::e_CREATE_DETACHED);
    attributes.setStackSize(STACK_SIZE);

    char initValue = 1;
    bdlqq::ThreadUtil::Handle handle;
    bdlqq::ThreadUtil::create(&handle,
                             attributes,
                             mySmallStackThreadFunction,
                             &initValue);
}

static
const char *policyToString(Attr::SchedulingPolicy policy)
{
    if      (Attr::e_SCHED_OTHER   == policy) {
        return "other";                                               // RETURN
    }
    else if (Attr::e_SCHED_FIFO    == policy) {
        return "fifo";                                                // RETURN
    }
    else if (Attr::e_SCHED_RR      == policy) {
        return "rr";                                                  // RETURN
    }
    else if (Attr::e_SCHED_DEFAULT == policy) {
        return "default";                                             // RETURN
    }
    else {
        ASSERT(0);
        return "unrecognized policy";                                 // RETURN
    }
}

static
bsl::ostream& operator<<(bsl::ostream&             stream,
                         const bsl::multiset<int>& thisSet)
{
    typedef bsl::multiset<int>::const_iterator CI;

    const CI b = thisSet.begin();
    const CI e = thisSet.end();

    const char *sep = "";
    for (CI it = b; e != it; ++it) {
        stream << sep << *it;
        sep = ", ";
    }

    return stream;
}

//-----------------------------------------------------------------------------
//                    Multipriority Effectiveness Test Case
//
// Thread priorities are only usable on some platforms (see component doc), and
// even on those, priorities only seem to make any difference when multiple
// threads are waiting on a mutex and the os is deciding which of them is
// chosen to run first.  So priorities are not really testable until you have
// a mutex type.  Since it was felt undesirable to do the test in the mutex
// component, we implement our own mutex class here.
//-----------------------------------------------------------------------------

namespace MULTIPRIORITY_EFFECTIVENESS_TEST_CASE {

#ifdef BSLS_PLATFORM_OS_WINDOWS

// Platform-specific implementation starts here.

// Rather than setting 'WINVER' or 'NTDDI_VERSION', just forward declare the
// Windows 2000 functions that are used.

struct _RTL_CRITICAL_SECTION;

typedef struct _RTL_CRITICAL_SECTION CRITICAL_SECTION, *LPCRITICAL_SECTION;
typedef int BOOL;
typedef unsigned long DWORD;

extern "C" {
    __declspec(dllimport) BOOL __stdcall InitializeCriticalSectionAndSpinCount(
                                    LPCRITICAL_SECTION lpCriticalSection,
                                    DWORD dwSpinCount);

    __declspec(dllimport) void __stdcall DeleteCriticalSection(
                                LPCRITICAL_SECTION lpCriticalSection);

    __declspec(dllimport) void __stdcall EnterCriticalSection(
                                LPCRITICAL_SECTION lpCriticalSection);

    __declspec(dllimport) void __stdcall LeaveCriticalSection(
                                LPCRITICAL_SECTION lpCriticalSection);

}  // extern "C"

class MyMutex {
    // It provides an efficient proxy for Windows critical sections,
    // and related operations.  Note that the MyMutex implemented in this class
    // is *not* error checking, and is non-recursive.

  public:
    enum {
        // Size of the buffer allocated for the critical section, in
        // pointer-sized elements.  We have to make it public so we could
        // access it in a .cpp file to verify the size.

#ifdef BSLS_PLATFORM_CPU_64_BIT
        // 5*8 = 40 bytes
        CRITICAL_SECTION_BUFFER_SIZE = 5
#else
        // 6*4 = 24 bytes
        CRITICAL_SECTION_BUFFER_SIZE = 6
#endif
    };

  private:
    enum {
        // A Windows critical section has a configurable spin count.  A lock
        // operation spins this many iterations (on, presumably, some atomic
        // integer) before sleeping on the underlying primitive.

        BCEMT_SPIN_COUNT = 30
    };

    // DATA
    void *d_lock[CRITICAL_SECTION_BUFFER_SIZE];

  private:
    // NOT IMPLEMENTED
    MyMutex(const MyMutex&);
    MyMutex& operator=(const MyMutex&);

  public:
    // CREATORS
    MyMutex()
        // Create a mutex initialized to an unlocked state.
    {
        InitializeCriticalSectionAndSpinCount(
          reinterpret_cast<_RTL_CRITICAL_SECTION *>(d_lock), BCEMT_SPIN_COUNT);
    }

    ~MyMutex()
        // Destroy this mutex object.
    {
        DeleteCriticalSection(
                             reinterpret_cast<_RTL_CRITICAL_SECTION*>(d_lock));
    }

    // MANIPULATORS
    void lock()
        // Acquire a lock on this mutex object.  If this object is currently
        // locked, then suspend execution of the current thread until a
        // lock can be acquired.  Note that the behavior is undefined if the
        // calling thread already owns the lock on this mutex, and will likely
        // result in a deadlock.
    {
        EnterCriticalSection(reinterpret_cast<_RTL_CRITICAL_SECTION*>(d_lock));
    }

    void unlock()
        // Release a lock on this mutex that was previously acquired through a
        // successful call to 'lock', or 'tryLock'.  The behavior is
        // undefined, unless the calling thread currently owns the lock on this
        // mutex.
    {
        LeaveCriticalSection(reinterpret_cast<_RTL_CRITICAL_SECTION*>(d_lock));
    }
};

#else
// Unix -- pthreads

class MyMutex {
    // This class provides a full specialization of 'MyMutex' for
    // pthreads.  It provides a efficient proxy for the 'pthread_mutex_t'
    // pthreads type, and related operations.  Note that the mutex implemented
    // in this class is *not* error checking, and is non-recursive.

    // DATA
    pthread_mutex_t d_lock;

    // NOT IMPLEMENTED
    MyMutex(const MyMutex&);
    MyMutex& operator=(const MyMutex&);

  public:
    // CREATORS
    MyMutex() { pthread_mutex_init(&d_lock, 0); }

    ~MyMutex() { pthread_mutex_destroy(&d_lock); }

    // MANIPULATORS
    void lock() { pthread_mutex_lock(&d_lock); }

    void unlock() { pthread_mutex_unlock(&d_lock); }
};

#endif

enum { NUM_NOT_URGENT_THREADS = 128,
       NUM_THREADS            = NUM_NOT_URGENT_THREADS + 1,
       URGENT_THREAD          = NUM_THREADS / 2 };

struct Functor {
    bool                  d_urgent;
    static int            s_urgentPlace;
    static bool           s_firstThread;
    static bsls::AtomicInt s_lockCount;
    static bsls::AtomicInt s_finished;
    static bsls::AtomicInt s_timerCounter;
    static MyMutex        s_mutex;

    // CREATORS
    Functor() : d_urgent(false) {}

    // ACCESSORS
    void operator()();
};
int            Functor::s_urgentPlace;
bool           Functor::s_firstThread = 1;
bsls::AtomicInt Functor::s_finished(0);
bsls::AtomicInt Functor::s_lockCount(0);
bsls::AtomicInt Functor::s_timerCounter(0);
MyMutex        Functor::s_mutex;

void Functor::operator()()
{
    enum {  LIMIT = 512,

            // 'TIMER'_MASK controls how often threads wait for other threads
            // to pile up against the mutex.  A lower value of 'TIMER_MASK'
            // means more frequent sleeps.  Solaris is more sloppy about
            // priroities and sleeps are needed more frequently.

#if defined(BSLS_PLATFORM_OS_SOLARIS)
            TIMER_MASK =  4 * LIMIT - 1
#else
            TIMER_MASK = 64 * LIMIT - 1
#endif
    };

    for (int i = 0; i < LIMIT; ++i) {
        ++s_lockCount;
        s_mutex.lock();
        if (s_firstThread) {
            s_firstThread = false;
            s_timerCounter = 0;

            // Sleep until all the other threads are blocked on the mutex.
            //
            // Careful!  This could take 2 seconds to wake up!

            while (s_lockCount < NUM_THREADS) {
                bdlqq::ThreadUtil::yield();
                bdlqq::ThreadUtil::microSleep(200 * 1000);
            }
        }
        bdlqq::ThreadUtil::yield();

        // Infrequently have the thread that's holding the lock sleep a little
        // to wait for the other threads to block, controlled by 'TIMER_MASK'.

        if ((++s_timerCounter & TIMER_MASK) == 0) {
            int lastLockCount;
            if ((lastLockCount = s_lockCount) < NUM_THREADS) {
                bdlqq::ThreadUtil::microSleep(10 * 1000);
                if (s_lockCount == lastLockCount) {
                    bdlqq::ThreadUtil::yield();
                }
            }
        }
        s_mutex.unlock();

        --s_lockCount;
    }

    if (d_urgent) {
        s_urgentPlace = s_finished;
    }
    ++s_finished;
}

}  // close namespace MULTIPRIORITY_EFFECTIVENESS_TEST_CASE

//-----------------------------------------------------------------------------
//                       Thread Policy Creation Test Case
//-----------------------------------------------------------------------------

namespace BCEMT_THREAD_POLICY_CREATION_TEST {

struct Touch {
    // functor

    // DATA
    bool *d_finished;

    void operator()()
        // When called, touches '*d_finished' to prove it ran, and returns.
    {
        ASSERT(! *d_finished);
        *d_finished = true;
    }
};

}  // close namespace BCEMT_THREAD_POLICY_CREATION_TEST

//-----------------------------------------------------------------------------
//                       Multipriority Usage Test Case
//-----------------------------------------------------------------------------

namespace MULTIPRIORITY_USAGE_TEST_CASE {

// Note that in practice, thread priorities only seem to make a difference
// when multiple stopped threads are simultaneously ready to run, so we won't
// see any effect of the different priorities in this case.

struct MostUrgentThreadFunctor {
    void operator()() const
    {
        if (verbose) {
            bsl::printf("Most urgent\n");
        }
    }
};

struct FairlyUrgentThreadFunctor {
    void operator()() const
    {
        if (verbose) {
            bsl::printf("Fairly urgent\n");
        }
    }
};

struct LeastUrgentThreadFunctor {
    void operator()() const
    {
        if (verbose) {
            bsl::printf("Least urgent\n");
        }
    }
};

}  // close namespace MULTIPRIORITY_USAGE_TEST_CASE

//-----------------------------------------------------------------------------
//                          CONFIGURATION TEST CASE
//-----------------------------------------------------------------------------

namespace BCEMT_CONFIGURATION_TEST_NAMESPACE {

struct Func {
    int         d_stackToUse;
    char       *d_lastBuf;
    static bool s_success;

    void recurser(char *base);
        // Recurse to create depth on stack

    void operator()();
        // Initialize, then call recurser, then set 'd_success'
};
bool Func::s_success;

void Func::recurser(char *base)
    // Consume greater than 'd_stackToUse' of stack depth
{
    char buf[5 * 1000];

    ASSERT(buf != d_lastBuf);    // make sure optimizer didn't remove recursion
    d_lastBuf = buf;

    bsl::memset(buf, 'a', sizeof(buf));

    if   (intPtrAbs(buf - base) < d_stackToUse
       && intPtrAbs(buf + sizeof(buf) - base) < d_stackToUse) {
        recurser(base);
    }

    for (char *pc = buf; pc < buf + sizeof(buf); ++pc) {
        ASSERT(*pc == 'a');
    }
}

void Func::operator()()
{
    if (verbose) P(d_stackToUse);

    d_lastBuf = 0;
    char base;
    recurser(&base);

    s_success = true;
}

}  // close namespace BCEMT_CONFIGURATION_TEST_NAMESPACE


extern "C"
void *configurationTestFunction(void *stackToUse)
{
    BCEMT_CONFIGURATION_TEST_NAMESPACE::Func func;

    func.d_stackToUse = (int) (bsls::Types::IntPtr) stackToUse;
    func.s_success   = false;

    func();

    ASSERT(func.d_stackToUse == (int) (bsls::Types::IntPtr) stackToUse);
    ASSERT(func.s_success);

    return 0;
}

//-----------------------------------------------------------------------------
//                             STACKSIZE TEST CASE
//-----------------------------------------------------------------------------

namespace STACKSIZE_TEST_CASE_NAMESPACE {

template <int BUFFER_SIZE>
struct Func {
    void operator()()
    {
        char buffer[BUFFER_SIZE == 0 ? 1 : BUFFER_SIZE];

        bsl::memset(buffer, 'a', sizeof(buffer));
    }

    static
    void staticFunc()
    {
        Func func;
        func();
    }
};

typedef void (*CppFuncPtr)();

extern "C"
void *callCppFunction(void *function)
{
    CppFuncPtr funcPtr = (CppFuncPtr) function;

    (*funcPtr)();

    return 0;
}

template <int BUFFER_SIZE>
void testStackSize()
{
#if defined(BSLS_PLATFORM_OS_UNIX) && !defined(BSLS_PLATFORM_OS_CYGWIN)
    // In test cases -2 and -4, Linux was crashing about 4K away from the stack
    // in 32 & 64 bit.  All other unix platforms were running past the end of
    // the stack without crashing.

    enum { FUDGE_FACTOR = 8192 };
#else
    // In test case -4, the crash on 32 bit was further than 12K away from
    // the end of the stack stack, on 64 bit it was further than 16k away.

    enum { FUDGE_FACTOR = 8192 + 2048 * sizeof(void *) };
#endif

    bdlqq::ThreadAttributes attr;
    attr.setStackSize(BUFFER_SIZE + FUDGE_FACTOR);
    attr.setGuardSize(MIN_GUARD_SIZE);

    Obj::Handle handle;

    int cRc = Obj::create(&handle, attr, Func<BUFFER_SIZE>());
    LOOP_ASSERT(BUFFER_SIZE, 0 == cRc);
    int jRc = 0;
    if (0 == cRc) {
        jRc = Obj::join(handle);
        ASSERT(0 == jRc);
    }

    if (verbose) {
        cout << "testStackSize<" << BUFFER_SIZE << ">() " <<
                                    ((cRc | jRc) ? "failed\n" : "succeeded\n");
    }

    cRc = Obj::create(&handle,
                      attr,
                      &callCppFunction,
                      (void *) &Func<BUFFER_SIZE>::staticFunc);
    LOOP_ASSERT(BUFFER_SIZE, 0 == cRc);
    jRc = 0;
    if (0 == cRc) {
        jRc = Obj::join(handle);
        ASSERT(0 == jRc);
    }

    if (verbose) {
        cout << "testStackSize<" << BUFFER_SIZE << ">::staticFunc() " <<
                                    ((cRc | jRc) ? "failed\n" : "succeeded\n");
    }
}

}  // close namespace STACKSIZE_TEST_CASE_NAMESPACE

//-----------------------------------------------------------------------------
//                              TEST CASE 6
//-----------------------------------------------------------------------------

extern "C" {

static
void TlsDestructor6_1(void *);
static
void TlsDestructor6_2(void *);

}  // extern "C"

namespace BCEMT_THREADUTIL_TLSKEY_TEST6 {

Obj::Key parentKey1;
Obj::Key parentKey2;
bsls::AtomicInt terminations1;
bsls::AtomicInt terminations2;

struct TlsKeyTestFunctor {
    int d_seed;

    // CREATOR
    explicit
    TlsKeyTestFunctor(int seed) : d_seed(seed) {}

    // ACCESSORS
    void operator()() const;
};

void TlsKeyTestFunctor::operator()() const
{
    namespace TC = BCEMT_THREADUTIL_TLSKEY_TEST6;

    int rc;

    ASSERT(0 == Obj::getSpecific(TC::parentKey1));
    ASSERT(0 == Obj::getSpecific(TC::parentKey2));

    rc = Obj::setSpecific(TC::parentKey1, (void *)  (5 * d_seed));
    ASSERT(0 == rc);
    rc = Obj::setSpecific(TC::parentKey2, (void *) (12 * d_seed));
    ASSERT(0 == rc);

    ASSERT((void *)  (5 * d_seed) == Obj::getSpecific(TC::parentKey1));
    ASSERT((void *) (12 * d_seed) == Obj::getSpecific(TC::parentKey2));

    rc = Obj::setSpecific(TC::parentKey1, (void *) 0);
    ASSERT(0 == rc);

    return;
}

}  // close namespace BCEMT_THREADUTIL_TLSKEY_TEST6

extern "C" {

static
void TlsDestructor6_1(void *)
{
    namespace TC = BCEMT_THREADUTIL_TLSKEY_TEST6;

    ++ TC::terminations1;
}

static
void TlsDestructor6_2(void *)
{
    namespace TC = BCEMT_THREADUTIL_TLSKEY_TEST6;

    ++ TC::terminations2;
}

}  // extern "C"

//-----------------------------------------------------------------------------
//                              TEST CASE 5
//-----------------------------------------------------------------------------

extern "C" {

static
void createKeyTestDestructor5(void *);

}  // extern "C"

namespace BCEMT_THREADUTIL_CREATEKEY_TEST5 {

Obj::Id childId;
Obj::Key parentKey;
Obj::Key childKey1;
Obj::Key childKey2;
int terminated = 0;

struct CreateKeyTestFunctor {
    bool d_doDestructor;    // do the destructor if and only if this is 'true'

    // CREATORS
    explicit
    CreateKeyTestFunctor(bool doDestructor) : d_doDestructor(doDestructor) {}

    // ACCESSORS
    void operator()() const;
};

void CreateKeyTestFunctor::operator()() const
{
    namespace TC = BCEMT_THREADUTIL_CREATEKEY_TEST5;

    TC::childId = Obj::selfId();

    int rc = Obj::createKey(&TC::childKey1,
                            d_doDestructor ? &createKeyTestDestructor5 : 0);
    ASSERT(0 == rc);
    ASSERT(TC::parentKey != TC::childKey1);
    ASSERT(0 == Obj::getSpecific(TC::childKey1));

    rc = Obj::setSpecific(TC::childKey1, (void *) 1);
    ASSERT(0 == rc);

    ASSERT((void *) 1 == Obj::getSpecific(TC::childKey1));

    rc = Obj::createKey(&TC::childKey2, &createKeyTestDestructor5);
    ASSERT(0 == rc);
    ASSERT(TC::childKey2 != TC::childKey1);
    ASSERT(TC::childKey2 != TC::parentKey);
    ASSERT(0 == Obj::getSpecific(TC::childKey2));

    rc = Obj::setSpecific(TC::childKey2, (void *) (d_doDestructor ? 1 : 0));
    ASSERT(0 == rc);

    ASSERT((void *) (d_doDestructor ? 1 : 0) ==
                                              Obj::getSpecific(TC::childKey2));

    return;
}

}  // close namespace BCEMT_THREADUTIL_CREATEKEY_TEST5

extern "C" {

static
void createKeyTestDestructor5(void *data)
{
    namespace TC = BCEMT_THREADUTIL_CREATEKEY_TEST5;

    ASSERT(Obj::selfId() == TC::childId);

    if (TC::terminated < 2)  {
        ASSERT((void *) 1 == data);
        int zeroCount = (0 == Obj::getSpecific(TC::childKey1)) +
                        (0 == Obj::getSpecific(TC::childKey2));
#ifdef BDLQQ_PLATFORM_POSIX_THREADS
        ASSERT(zeroCount == TC::terminated + 1);
#else
        ASSERT(0 == zeroCount);
#endif
    }
    else {
#ifdef BDLQQ_PLATFORM_POSIX_THREADS
        ASSERT((void *) 5 == data);
#else
        ASSERT(0);
#endif
    }

    if (1 == TC::terminated) {
        int rc = Obj::setSpecific(TC::childKey2, (void *) 5);
        ASSERT(0 == rc);
    }

    ++ TC::terminated;
}

}  // extern "C"

inline
long mymax(long a, long b)
{
    return a >= b ? a : b;
}

inline
long myAbs(long a)
{
    return a >= 0 ? a : -a;
}

void testCaseMinus1Recurser(const char *start)
{
    enum { BUF_LEN = 1024 };
    char buffer[BUF_LEN];
    static double lastDistance = 1;

    double distance = (double) mymax(myAbs(&buffer[0]           - start),
                                     myAbs(&buffer[BUF_LEN - 1] - start));
    if (distance / lastDistance > 1.02) {
        cout << (int) distance << endl << flush;
        lastDistance = distance;
    }

    testCaseMinus1Recurser(start);
}

extern "C" void *testCaseMinus1ThreadMain(void *)
{
    char c = 0;
    testCaseMinus1Recurser(&c);

    return 0;
}

//-----------------------------------------------------------------------------
//                                  TEST CASE -2
//-----------------------------------------------------------------------------

#ifndef BSLS_PLATFORM_OS_WINDOWS

enum { CLEARANCE_TEST_START  = 0,
       CLEARANCE_TEST_DONE   = 1234,
       CLEARANCE_BUFFER_SIZE = 64 * 1024 - 600 };

static int clearanceTestState;
static int clearanceTestAllocaSize;

extern "C" void *clearanceTest(void *)
{
    if (clearanceTestAllocaSize) {
        char *pc = (char *) alloca(clearanceTestAllocaSize);
        ASSERT(pc);

        pc[0] = 0;
        ++pc[0];
        pc[clearanceTestAllocaSize - 1] = 0;
        ++pc[clearanceTestAllocaSize - 1];
    }

    clearanceTestState = CLEARANCE_TEST_DONE;

    return 0;
}

#endif

//-----------------------------------------------------------------------------
//                                  TEST CASE -3
//-----------------------------------------------------------------------------

bool stackGrowthIsNegative(char *pc)
{
    char c;

    return &c < pc;
}

//-----------------------------------------------------------------------------
//                                  TEST CASE -4
//-----------------------------------------------------------------------------

extern "C" void *secondClearanceTest(void *vStackSize)
{
    static int growth;
    char c;
    growth = stackGrowthIsNegative(&c) ? -10 : 10;

    static int stackSize;
    stackSize = (int) (bsls::Types::IntPtr) vStackSize;

    static char *pc;
    pc = &c;

    static int diff;

    for (;; pc += growth) {
        *pc = 0;
        diff =  stackSize - (int) myAbs(pc - &c);

        printf("%d\n", diff);
    }

    return 0;
}

//=============================================================================
//                                  MAIN PROGRAM
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    int test = argc > 1 ? atoi(argv[1]) : 0;
    verbose = argc > 2;
    veryVerbose = argc > 3;
    veryVeryVerbose = argc > 4;

    cout << "TEST " << __FILE__ << " CASE " << test << endl;

#if defined(BSLS_PLATFORM_OS_SOLARIS)
    // Determine whether we are after Solaris 5.10.

    struct utsname utsName;
    uname(&utsName);

    int front, tail = 0;
    char *dot = bsl::strchr(utsName.release, '.');
    if (dot) {
        *dot = 0;
        tail = bsl::atoi(dot + 1);
    }
    front = bsl::atoi(utsName.release);

    const bool isPost_5_10 = front > 5 || (5 == front && tail > 10);
#endif

    switch (test) { case 0:  // Zero is always the leading case.
      case 15: {
        // --------------------------------------------------------------------
        // CREATE ALLOCATION TEST
        //
        // Concerns:
        //   Get the 'create' function to stop using the global allocator on
        //   Unix.  Note this concern is only relevant to the 'create' methods
        //   that take a functor, the ones that take a function pointer don't
        //   do any memory allocation.
        //
        // Plan:
        //   Create a thread with each of the relevant 'create' functions,
        //   passing a functor that is very large that will necessitate memory
        //   allocation.  Observe that the default and global allocators are
        //   not used.
        // --------------------------------------------------------------------

        if (verbose) cout << "CREATE ALLOCATION TEST\n"
                             "======================\n";

        bslma::TestAllocator da;
        bslma::TestAllocator ga;
        bslma::TestAllocator ta;

        bslma::Default::setDefaultAllocator(&da);
        bslma::Default::setGlobalAllocator(&ga);

        bdlqq::ThreadAttributes attr;
        attr.setStackSize(10 << 10);    // smaller than the functor object

        bdlqq::ThreadUtil::Handle handles[2];
        int rc;

        rc = Obj::create(&handles[0],
                         attr,
                         BigFunctor(),
                         &ta);
        ASSERT(0 == rc);

        rc = Obj::create(&handles[1],
                         BigFunctor(),
                         &ta);
        ASSERT(0 == rc);

        rc = Obj::join(handles[0]);
        ASSERT(0 == rc);

        rc = Obj::join(handles[1]);
        ASSERT(0 == rc);

        ASSERT(0 == ga.numAllocations());
        ASSERT(0 != ta.numAllocations());

        // 64 bit windows uses the global allocator for an internal map of
        // threads, which we don't plan to fix.

#if !defined(BSLS_PLATFORM_OS_WINDOWS) || !defined(BSLS_PLATFORM_CPU_64_BIT)
        ASSERT(0 == ga.numAllocations());
#endif
      } break;
      case 14: {
        // --------------------------------------------------------------------
        // PRIORITY EFFECTIVENESS TEST
        //
        // Concerns:
        //: 1 We want to observe thread priorities making a difference.  It has
        //:   been empirically determined that rarely will a more urgent thread
        //:   interrupt a less urgent thread that is running, the only place
        //:   where it seems to make a difference is when the threads are both
        //:   blocked -- then priority seems to make a difference in which one
        //:   is allowed to run first.
        //: 2 This test verifies that higher numbers signify more urgent
        //:   priorities.  The pthreads doc never seems to make that clear.
        //:   This test is to verify that priorities never work 'backwards' --
        //:   that is, whether we expect priorities to work or not, never do
        //:   lower-numbered priorities get treated with significantly more
        //:   urgency than higher numbered priorities.
        //: 3 We also verify that on platforms where the component doc promises
        //:   that priorities will work, they do work, and that a thread
        //:   specified as more urgent will complete a certain task more
        //:   quickly than less urgent threads.
        //
        // Plan:
        //: 1 We create a scenario where a large number of threads are
        //:   contending many times for a single mutex.  One of these threads
        //:   has max pirority, all the others have min priority.  We verify
        //:   that the one thread with max priority is successful at getting
        //:   more rapid access to the mutex.  We have an atomic variable
        //:   's_finished' which counts how many threads have finished aquiring
        //:   the mutex a large number of times.  When the urgent thread
        //:   finishes, it sets the value 's_urgentPlace' to the value of
        //:   's_finished' when it finished, so after all threads have
        //:   finished, 's_urgentPlace' tells us how many threads finished
        //:   before the urgent thread.
        //: 2 We do a number of trials, inserting the value of 's_urgentPlace'
        //:   into a multiset each time.
        //: 3 If priorities are working backwards (C-2), the best value of
        //:   's_urgentPlace' (which will be at the front of the multiset) will
        //:   place among the last threads to complete.  Verify that didn't
        //:   happen.
        //: 4 Examining the first few elements of this multiset tells us how
        //:   the best trials turned out (C-3):
        //:   o We confirm that 's_urgentPlace' was 0 for the best trial, and
        //:     near the front on subsequent trials.  The worst couple of
        //:     trials we ignore.
        //
        // Testing
        //: o Attr::setSchedulingPolicy
        //: o Attr::setSchedulingPriority
        // --------------------------------------------------------------------

        if (verbose) cout << "Thread Priorities Test\n"
                             "======================\n";

        namespace TC = MULTIPRIORITY_EFFECTIVENESS_TEST_CASE;

#if defined(BSLS_PLATFORM_OS_HPUX)
        break;    // Spawning threads fails on HPUX if 'inheritSchedule' is
                  // not 1
#endif

        typedef Attr::SchedulingPolicy Policy;

        static const struct {
            int    d_line;
            Policy d_policy;
        } DATA[] = {
            { L_, Attr::e_SCHED_DEFAULT },
            { L_, Attr::e_SCHED_OTHER   },
            { L_, Attr::e_SCHED_FIFO    },
            { L_, Attr::e_SCHED_RR      },
        };
        enum { DATA_LEN = sizeof(DATA) / sizeof(*DATA) };

        bsl::multiset<int> urgentPlaces[DATA_LEN];

        for (int i = 0; i < DATA_LEN; ++i) {
            const int    LINE   = DATA[i].d_line;
            const Policy POLICY = DATA[i].d_policy;

#if defined(BSLS_PLATFORM_OS_AIX) || defined(BSLS_PLATFORM_OS_LINUX)
            if (Attr::e_SCHED_FIFO == POLICY ||
                                              Attr::e_SCHED_RR == POLICY) {
                continue;
            }
#endif

            // Avoid redundant tests (sometimes 'OTHER' and 'DEFAULT' are the
            // same, ie Sun.

            {
                bool found = false;
                for (int j = 0; j < i; ++j) {
                    if (POLICY == DATA[j].d_policy) found = true;
                }
                if (found) continue;
            }

#if   defined(BSLS_PLATFORM_OS_SOLARIS)
            const int prioritiesWork = !isPost_5_10 ||
                                           (Attr::BCEMT_SCHED_FIFO != POLICY &&
                                            Attr::BCEMT_SCHED_RR   != POLICY);
#elif defined(BSLS_PLATFORM_OS_AIX)
            const int prioritiesWork = 1;
#else
            const int prioritiesWork = 0;
#endif

            const int MAX_PRIORITY = Obj::getMaxSchedulingPriority(POLICY);
            const int MIN_PRIORITY = Obj::getMinSchedulingPriority(POLICY);
            ASSERT(0 == prioritiesWork || MAX_PRIORITY > MIN_PRIORITY);
            ASSERT(MAX_PRIORITY >= MIN_PRIORITY);

            if (veryVerbose) {
                P_(POLICY); P_(MAX_PRIORITY); P(MIN_PRIORITY);
            }

            // If there's no difference between max and min priority, there's
            // no point in doing the test.

            if (MAX_PRIORITY == MIN_PRIORITY) {
                if (verbose) {
                    cout << "Policy " << policyToString(POLICY) <<
                            ", Max pri " << setw(3) << MAX_PRIORITY <<
                            ", Min pri " << setw(3) << MIN_PRIORITY << endl;
                }
                continue;
            }

            ASSERT(prioritiesWork);

            // Create two 'Attr' objects, an 'urgent' one with max priority,
            // and a 'notUrgent' one with min priority.

            Attr urgentAttr;;
            urgentAttr.setStackSize(64 * 1024);
            urgentAttr.setInheritSchedule(0);
            urgentAttr.setSchedulingPolicy(POLICY);
            urgentAttr.setSchedulingPriority(MAX_PRIORITY);

            Attr notUrgentAttr(urgentAttr);
            notUrgentAttr.setSchedulingPriority(MIN_PRIORITY);

            // Do twice as many trials if priorities don't work, because
            // Solaris (one of the platforms where priorities work) is the slow
            // platform, and a larger number of trials reduces the chance
            // probability (never 0) that the test that priorities killed us
            // will fail.

            const int numTrials = 5;

            for (int j = 0; j < numTrials; ++j) {
                TC::Functor::s_urgentPlace = -1;
                TC::Functor::s_finished    = 0;
                TC::Functor::s_firstThread = true;
                TC::Functor::s_lockCount   = 0;

                TC::Functor fs[TC::NUM_THREADS];
                fs[TC::URGENT_THREAD].d_urgent = true;

                Obj::Handle handles[TC::NUM_THREADS];
                int rc;
                int numThreads = 0;
                for ( ; numThreads < TC::NUM_THREADS; ++numThreads) {
                    errno = 0;
                    const Attr& attr = fs[numThreads].d_urgent
                                     ? urgentAttr
                                     : notUrgentAttr;
                    rc = Obj::create(&handles[numThreads],
                                     attr,
                                     fs[numThreads]);
                    LOOP4_ASSERT(LINE, rc, numThreads, errno, 0 == rc);
                    if (rc) {
                        break;
                    }
                }

                for (int j = 0; j < numThreads; ++j) {
                    rc = Obj::join(handles[j]);
                    LOOP3_ASSERT(LINE, rc, j, 0 == rc);
                    if (rc) {
                        break;
                    }
                }

                ASSERT(TC::Functor::s_urgentPlace >= 0);
                ASSERT(TC::Functor::s_urgentPlace < TC::NUM_THREADS);
                ASSERT(! TC::Functor::s_firstThread);
                ASSERT(0 == TC::Functor::s_lockCount);
                ASSERT(TC::NUM_THREADS == TC::Functor::s_finished);

                urgentPlaces[i].insert(TC::Functor::s_urgentPlace);  // P-2

                if (veryVeryVerbose) {
                    cout << "Policy " << policyToString(POLICY) <<
                            ", Max pri " << setw(3) << MAX_PRIORITY <<
                            ", Min pri " << setw(3) << MIN_PRIORITY <<
                            ", Place " << TC::Functor::s_urgentPlace << endl;
                }
            }

            ASSERT(numTrials == urgentPlaces[i].size());

            bool failed;
            typedef bsl::multiset<int>::const_iterator Iterator;
            Iterator       it  = urgentPlaces[i].begin();
            const Iterator end = urgentPlaces[i].end();

            // If priorities are slaughtering us, the urgent thread will always
            // be coming near last and '*it', the best performing of the
            // several trials, will have come within 5 of last. (P-3)

            LOOP2_ASSERT(*it, TC::NUM_THREADS,
                                       !(failed = *it >= TC::NUM_THREADS - 5));

            // Examine the multiset (P-4)

            int thresholdA = 2, thresholdB = 5, thresholdC = 10;
#if defined(BSLS_PLATFORM_OS_SOLARIS)
            if (isPost_5_10) {
                thresholdA = 50; thresholdB = 90; thresholdC = 120;
            }
#endif

            bool failA = 0, failB = 0, failC = 0;

            // The best performing trial (out of 5) will have the urgent thread
            // finish in 2nd place or before (out of 129 threads).

            LOOP2_ASSERT(urgentPlaces[i], thresholdA,
                                           (failA = *it > thresholdA, !failA));

            // The third best performing trial (out of 5) should have finished
            // in 5th place or before (out of 129 threads).

            ++it;   ++it;    ASSERT(end != it);
            LOOP2_ASSERT(urgentPlaces[i], thresholdB,
                                           (failB = *it > thresholdB, !failB));

            // The 4th best performing trial (out of 5) should have finished in
            // 10th place or before (out of 129 threads).

            ++it;            ASSERT(end != it);
            LOOP2_ASSERT(urgentPlaces[i], thresholdC,
                                           (failC = *it > thresholdC, !failC));

            // Verify there were 5 trials.  Should be true whether priorities
            // worked or not.

            ++it;    ASSERT(end == ++it);

            failed |= failA || failB || failC;

            if (veryVerbose || failed) {
                cout << "Urgent Places[" << policyToString(POLICY) << "]" <<
                                             "    " << urgentPlaces[i] << endl;
            }
        }
      }  break;
      case 13: {
        // --------------------------------------------------------------------
        // THREAD POLICY AND PRIORITY CREATION TEST
        //
        // Concerns:
        //: o Determine which policies and priority values it will be possible
        //:   create a thread at, and for which platform, and verify that our
        //:   component doc about which polices will work on which platofrm is
        //:   accurate.
        //
        // Plan:
        //: o Iterate through all possible thread policies.
        //: o For each policy, set the bool constant 'willFail' to indicate
        //:   whether the component doc predicts that thread creation will fail
        //:   for the current platform and policy.
        //: o Set a thread attributes object with the given policy.
        //: o Iterate variable 'priority' from the min to the max priority for
        //:   the given platform as specfied by 'Obj::getMinSchedulingPriority'
        //:   and 'Obj::getMaxSchedulingPriority'.
        //: o Attempt to spawn a thread running the 'Touch' functor, which if
        //:   it runs will set a boolean variable 'finished'.
        //: o Verify from the return code of 'Obj::create' that thread creation
        //:   either succeeded or failed as predicted by 'willFail'.
        //: o Verify from the value of 'finished' whether the spawned thread
        //:   indeed ran or not.
        //
        // Testing:
        //: o Obj::getMinSchedulingPriority
        //: o Obj::getMaxSchedulingPriority
        //: o Obj::create
        // --------------------------------------------------------------------

        if (verbose) cout << "Thread Policy and Priority Creation Test\n"
                             "========================================\n";

        namespace TC = BCEMT_THREAD_POLICY_CREATION_TEST;

        Attr::SchedulingPolicy policies[] = { Attr::e_SCHED_OTHER,
                                              Attr::e_SCHED_FIFO,
                                              Attr::e_SCHED_RR,
                                              Attr::e_SCHED_DEFAULT };
        enum { NUM_POLICIES = sizeof policies / sizeof *policies };

        for (int i = 0; i < NUM_POLICIES; ++i) {
            const Attr::SchedulingPolicy policy = policies[i];

#ifdef BSLS_PLATFORM_OS_HPUX
            const bool willFail = true;
#elif defined(BSLS_PLATFORM_OS_LINUX) || defined(BSLS_PLATFORM_OS_AIX)
            const bool willFail = (Attr::e_SCHED_FIFO == policy ||
                                   Attr::e_SCHED_RR   == policy);
#elif defined(BSLS_PLATFORM_OS_SOLARIS)
            const bool willFail = isPost_5_10 &&
                                           (Attr::BCEMT_SCHED_FIFO == policy ||
                                            Attr::BCEMT_SCHED_RR   == policy);
#elif defined(BSLS_PLATFORM_OS_WINDOWS) || defined(BSLS_PLATFORM_OS_DARWIN)
            const bool willFail = false;
#else
# error unrecognized platform
#endif

            const int minPri = Obj::getMinSchedulingPriority(policy);
            const int maxPri = Obj::getMaxSchedulingPriority(policy);
            ASSERT(willFail || minPri <= maxPri);

            if (verbose) {
                cout << policyToString(policy) << ": " <<
                        minPri << " - " << maxPri <<
                        (willFail ? " expected to fail\n"
                                  : " expected to succeed\n");
            }

            Attr attr;
            attr.setStackSize(50 * 1024);
            attr.setSchedulingPolicy(policy);
            attr.setInheritSchedule(0);

            bool finished;
            TC::Touch touch;
            touch.d_finished = &finished;

            for (int priority = minPri; priority <= maxPri; ++priority) {
                attr.setSchedulingPriority(priority);

                finished = false;

                errno = 0;
                Obj::Handle handle;
                int rc = Obj::create(&handle, attr, touch);
                if (willFail) {
                    LOOP2_ASSERT(policyToString(policy), priority,
                                              0 != rc && "unexpected success");
                }
                else {
                    LOOP4_ASSERT(policyToString(policy), priority, errno,
                                                                  rc, 0 == rc);
                }
                if (0 == rc) {
                    rc = Obj::join(handle);
                    ASSERT(0 == rc);
                    ASSERT(finished);
                }
                else {
                    Obj::yield();
                    ASSERT(!finished);
                }
            }
        }
      }  break;
      case 12: {
        // --------------------------------------------------------------------
        // TESTING: sleepUntil
        //
        // Note that this is a system-call wrapper, and this test is intended
        // to ensure the system call is correctly called by the
        // 'bcem_threadutil'.  This test specifically does *not* test the
        // accuracy of the underlying system call.  Also note that
        // due to the nature of the system call, testing values at the upper
        // bound of the valid range is not reasonable.  Test case -5, has been
        // created and run by hand to verify (slightly) longer time periods.
        //
        // Concerns:
        //: 1 'sleepUntil' suspends the current thread until the indicated
        //:    time in the future (within some reasonable limit).
        //:
        //: 2 'sleepUntil' does not suspend the current thread (or suspends it
        //:    very briefly), for the current time, or times in the past.
        //:
        //: 3 'sleepUntil' works as expected with the monotonic system clock.
        //:
        //: 4  QoI: Asserted precondition violations are detected when enabled.
        //
        // Plan:
        //: 1 Call 'sleepUntil' for a series of values less than a second in
        //:   the future, and verify that system time after sleeping is within
        //:   a reasonable range of the expected target time. (C-1)
        //:
        //: 2 Call 'sleepUntil' for a value in the past, and verify that
        //:   the function returns to the caller in a reasonably small amount
        //:   of time. (C-2)
        //:
        //: 3 Repeat these two tests for 'sleepUntil' with the monotonic
        //:   system clock specified.
        //:
        //: 4 Verify that, in appropriate build modes, defensive checks are
        //:   triggered for invalid time-interval values. (using the
        //:   'BSLS_ASSERTTEST_*' macros) (C-4)
        //
        // Testing:
        //   void sleepUntil(const bsls::TimeInterval& );
        // --------------------------------------------------------------------

        if (verbose) {
            cout << endl
                 << "CLASS METHOD 'sleepUntil'" << endl
                 << "=========================" << endl;
        }

        if (veryVerbose) {
            cout << "sleepUntil for times in the future" << endl;
        }

        for (int i = 0; i < 5; ++i) {
            if (veryVeryVerbose) {
                cout << "sleepUntil for " << i * 100 << "ms" << endl;
            }

            bsls::TimeInterval expectedTime;
            bsls::TimeInterval actualTime;
            {
                // realtime clock
                expectedTime = bdlt::CurrentTime::now();
                expectedTime.addMilliseconds(i * 100);
                Obj::sleepUntil(expectedTime);
                actualTime = bdlt::CurrentTime::now();
                ASSERT(actualTime >= expectedTime);
                LOOP_ASSERT((actualTime - expectedTime).totalMilliseconds(),
                            (actualTime - expectedTime).totalMilliseconds()
                                                                         < 50);
            }
            {
                // monotonic clock
                expectedTime = bsls::SystemTime::nowMonotonicClock();
                expectedTime.addMilliseconds(i * 100);
                Obj::sleepUntil(expectedTime,
                                bsls::SystemClockType::e_MONOTONIC);
                actualTime = bsls::SystemTime::nowMonotonicClock();

                // MS clock resolution is 15.6ms, include a fudge factor
                ASSERT(actualTime >= expectedTime - bsls::TimeInterval(.0156));
                LOOP_ASSERT((actualTime - expectedTime).totalMilliseconds(),
                            (actualTime - expectedTime).totalMilliseconds()
                                                                         < 50);
            }
        }

        if (veryVerbose) {
            cout << "sleepUntil for times in the past" << endl;
        }
        {
            bsls::TimeInterval expectedTime;
            bsls::TimeInterval actualTime;
            { // realtime clock
                expectedTime = bdlt::CurrentTime::now();
                Obj::sleepUntil(expectedTime - bsls::TimeInterval(1));
                actualTime = bdlt::CurrentTime::now();
                ASSERT(actualTime >= expectedTime);
                LOOP_ASSERT((actualTime - expectedTime).totalMilliseconds(),
                            (actualTime - expectedTime).totalMilliseconds()
                                                                         < 50);
            }
            { // monotonic clock
                expectedTime = bsls::SystemTime::nowMonotonicClock();
                Obj::sleepUntil(expectedTime - bsls::TimeInterval(1),
                                bsls::SystemClockType::e_MONOTONIC);
                actualTime = bsls::SystemTime::nowMonotonicClock();
                ASSERT(actualTime >= expectedTime);
                LOOP_ASSERT((actualTime - expectedTime).totalMilliseconds(),
                            (actualTime - expectedTime).totalMilliseconds()
                                                                         < 50);
            }
        }
        if (verbose) {
            cout << "Negative Testing." << endl;
        }
        {
            bsls::AssertFailureHandlerGuard hG(
                                             bsls::AssertTest::failTestDriver);

            // Note that we must use 'RAW' handlers as the assertions are
            // generated by implementation components.

            ASSERT_PASS(Obj::sleepUntil(bsls::TimeInterval(0)));
            ASSERT_FAIL_RAW(Obj::sleepUntil(bsls::TimeInterval(-1, 0)));
            ASSERT_FAIL_RAW(Obj::sleepUntil(bsls::TimeInterval(0, -1)));
            ASSERT_FAIL_RAW(Obj::sleepUntil(bsls::TimeInterval(253402300800LL,
                                                              0)));
        }
      }  break;
      case 11: {
        // --------------------------------------------------------------------
        // Usgae Example 3: MULTIPLE PRIORITY THREADS
        //
        // Concern:
        //   Need to demonstrate setting priorities for threads.  Note that we
        //   don't demonstrate the different priorities actually having an
        //   effect, we just demonstrate how to set them.
        //
        // Plan:
        //   Spawn 3 threads with different priorities.  Note that this test
        //   works on Windows even though priorities are ignored there, and on
        //   Linux even though the max priority equals the min priority.  This
        //   is because we don't do any testing of which thread runs first, in
        //   fact, since there are only 3 threads and they are all EXTREMELY
        //   short jobs, there probably won't be any contention for processors
        //   at all.  This is just to demonstrate the interface.
        // --------------------------------------------------------------------

#if defined(BSLS_PLATFORM_OS_HPUX) || defined(BSLS_PLATFORM_OS_CYGWIN)
        // Spawning threads fails on HP-UX and Cygwin unless
        // 'inheritSchedule == true'.

        if (verbose) {
            cout << "Skipping case 11 on HP-UX and Cygwin..." << endl;
        }
#else
        using namespace MULTIPRIORITY_USAGE_TEST_CASE;

        enum { NUM_THREADS = 3 };

        bdlqq::ThreadUtil::Handle handles[NUM_THREADS];
        bdlqq::ThreadUtil::Invokable functions[NUM_THREADS] = {
                                                  MostUrgentThreadFunctor(),
                                                  FairlyUrgentThreadFunctor(),
                                                  LeastUrgentThreadFunctor() };
        double priorities[NUM_THREADS] = { 1.0, 0.5, 0.0 };

        bdlqq::ThreadAttributes attributes;
        attributes.setInheritSchedule(false);
        const bdlqq::ThreadAttributes::SchedulingPolicy policy =
                                    bdlqq::ThreadAttributes::e_SCHED_OTHER;
        attributes.setSchedulingPolicy(policy);

        for (int i = 0; i < NUM_THREADS; ++i) {
            attributes.setSchedulingPriority(
                 bdlqq::ThreadUtil::convertToSchedulingPriority(policy,
                                                               priorities[i]));
            int rc = bdlqq::ThreadUtil::create(&handles[i],
                                              attributes,
                                              functions[i]);
            ASSERT(0 == rc);
        }

        for (int i = 0; i < NUM_THREADS; ++i) {
            int rc = bdlqq::ThreadUtil::join(handles[i]);
            ASSERT(0 == rc);
        }
#endif
      }  break;
      case 10: {
        // --------------------------------------------------------------------
        // BCEMT_CONFIGURATION TEST
        //
        // Concern:
        //   That bdlqq::Configuration can really affect stack size.
        //
        // Plan:
        //   Configure a stack size several times the native default, then
        //   verify the stack is at least about that size.  If the stack is
        //   only about the native size, we should get a stack overflow.
        // --------------------------------------------------------------------

        const int stackSize =
                      5 * bdlqq::Configuration::nativeDefaultThreadStackSize();
        const int stackToUse =
                      4 * bdlqq::Configuration::nativeDefaultThreadStackSize();
        bdlqq::Configuration::setDefaultThreadStackSize(stackSize);

        bdlqq::ThreadUtil::Handle handle;

        if (verbose) Q(Test functor with no attributes);
        {
            BCEMT_CONFIGURATION_TEST_NAMESPACE::Func func;

            func.d_stackToUse = stackToUse;
            func.s_success    = false;

            ASSERT(func.d_stackToUse >
                     3 * bdlqq::Configuration::nativeDefaultThreadStackSize());

            int rc = bdlqq::ThreadUtil::create(&handle, func);
            ASSERT(0 == rc);

            rc = bdlqq::ThreadUtil::join(handle);
            ASSERT(0 == rc);

            ASSERT(func.s_success);
            ASSERT(func.d_stackToUse == stackToUse);
        }

        if (verbose) Q(Test functor with default attributes);
        {
            BCEMT_CONFIGURATION_TEST_NAMESPACE::Func func;

            func.d_stackToUse = stackToUse;
            func.s_success    = false;

            bdlqq::ThreadAttributes attr;
            int rc = bdlqq::ThreadUtil::create(&handle, attr, func);
            ASSERT(0 == rc);

            rc = bdlqq::ThreadUtil::join(handle);
            ASSERT(0 == rc);

            ASSERT(func.s_success);
            ASSERT(func.d_stackToUse == stackToUse);
        }

        if (verbose) Q(Test C function with no attributes);
        {
            int rc = bdlqq::ThreadUtil::create(
                                    &handle,
                                    &configurationTestFunction,
                                    (void *) (bsls::Types::IntPtr) stackToUse);
            ASSERT(0 == rc);

            rc = bdlqq::ThreadUtil::join(handle);
            ASSERT(0 == rc);
        }

        if (verbose) Q(Test C function with default attributes object);
        {
            bdlqq::ThreadAttributes attr;
            int rc = bdlqq::ThreadUtil::create(
                                    &handle,
                                    attr,
                                    &configurationTestFunction,
                                    (void *) (bsls::Types::IntPtr) stackToUse);
            ASSERT(0 == rc);

            rc = bdlqq::ThreadUtil::join(handle);
            ASSERT(0 == rc);
        }
      }  break;
      case 9: {
        // --------------------------------------------------------------------
        // TESTING: convertToSchedulingPriority
        //
        // Concern:
        //   That 'convertToSchedulingPriority' works as specced.
        //
        // Plan:
        //   Call 'get{Min,Max}SchedPriority' and compare the results they
        //   return to results reterned by 'convertToSchedulingPriority'.
        // --------------------------------------------------------------------

        typedef bdlqq::ThreadAttributes Attr;

        Attr::SchedulingPolicy policies[] = { Attr::e_SCHED_OTHER,
                                              Attr::e_SCHED_FIFO,
                                              Attr::e_SCHED_RR,
                                              Attr::e_SCHED_DEFAULT };
        enum { NUM_POLICIES = sizeof policies / sizeof *policies };

        for (int i = 0; i < NUM_POLICIES; ++i) {
            const Attr::SchedulingPolicy POLICY = policies[i];

            const int minPri = Obj::getMinSchedulingPriority(POLICY);
            const int maxPri = Obj::getMaxSchedulingPriority(POLICY);

            if (veryVerbose) {
                P_(policyToString(POLICY)); P_(minPri); P(maxPri);
            }

            const int loPri =  Obj::convertToSchedulingPriority(POLICY, 0.0);
            LOOP2_ASSERT(loPri, minPri, loPri == minPri);

            const int midPri = Obj::convertToSchedulingPriority(POLICY, 0.5);
#if !defined(BSLS_PLATFORM_OS_CYGWIN)
            LOOP2_ASSERT(midPri, minPri, midPri >= minPri);
            LOOP2_ASSERT(midPri, maxPri, midPri <= maxPri);
#else
            LOOP2_ASSERT(midPri, minPri, midPri <= minPri);
            LOOP2_ASSERT(midPri, maxPri, midPri >= maxPri);
#endif

            const int hiPri =  Obj::convertToSchedulingPriority(POLICY, 1.0);
            LOOP2_ASSERT(hiPri, maxPri, hiPri == maxPri);

            if (hiPri != loPri) {
#if !defined(BSLS_PLATFORM_OS_CYGWIN)
                LOOP2_ASSERT(hiPri,  loPri, hiPri >= loPri + 2);

                LOOP2_ASSERT(midPri, hiPri, midPri < hiPri);
                LOOP2_ASSERT(midPri, loPri, midPri > loPri);
#else
                LOOP2_ASSERT(hiPri,  loPri, hiPri <= loPri + 2);

                LOOP2_ASSERT(midPri, hiPri, midPri > hiPri);
                LOOP2_ASSERT(midPri, loPri, midPri < loPri);
#endif
            }
            else {
#if defined(BSLS_PLATFORM_OS_SOLARIS)
                ASSERT(Attr::BCEMT_SCHED_FIFO == POLICY ||
                       Attr::BCEMT_SCHED_RR   == POLICY);
#elif !defined(BSLS_PLATFORM_OS_LINUX) && !defined(BSLS_PLATFORM_OS_WINDOWS)
                // This should only happen on Linux and Windows
                ASSERT(0);
#endif

                LOOP2_ASSERT(midPri, hiPri, midPri == hiPri);
                LOOP2_ASSERT(midPri, loPri, midPri == loPri);
            }
        }
      }  break;
      case 8: {
        // --------------------------------------------------------------------
        // STACK SIZE
        //
        // Concern:
        //   Does setting 'stackSize' allow one to create a buffer of nearly
        //   that size in the thread?
        //
        // Plan:
        //   For various stack sizes, create threads with that stack size and
        //   create a buffer of nearly that size in the thread (minus a
        //   platform-dependent fudge factor), and see if we crash.  Note that
        //   this test is not guaranteed to fail if there is a problem -- it is
        //   possible that if there is a bug and invalid memory access occurs,
        //   it will just abuse the heap but not result in a crash.
        // --------------------------------------------------------------------

        namespace TC = STACKSIZE_TEST_CASE_NAMESPACE;

        enum { K = 1024 };

        if (verbose) {
#ifdef PTHREAD_STACK_MIN
            P(PTHREAD_STACK_MIN);
#else
            cout << "'PTHREAD_STACK_MIN' undefined\n";
#endif
        }

        TC::testStackSize<    0    >();
        TC::testStackSize<    1 * K>();
        TC::testStackSize<    2 * K>();
        TC::testStackSize<    3 * K>();
        TC::testStackSize<    4 * K>();
        TC::testStackSize<    7 * K>();
        TC::testStackSize<    8 * K>();
        TC::testStackSize<    9 * K>();
        TC::testStackSize<   10 * K>();
        TC::testStackSize<   12 * K>();
        TC::testStackSize<   14 * K>();
        TC::testStackSize<   15 * K>();
        TC::testStackSize<   16 * K>();
        TC::testStackSize<   17 * K>();
        TC::testStackSize<   18 * K>();
        TC::testStackSize<   20 * K>();
        TC::testStackSize<   24 * K>();
        TC::testStackSize<   28 * K>();
        TC::testStackSize<   31 * K>();
        TC::testStackSize<   32 * K>();
        TC::testStackSize<   33 * K>();
        TC::testStackSize<   36 * K>();
        TC::testStackSize<   40 * K>();
        TC::testStackSize<   44 * K>();
        TC::testStackSize<   48 * K>();
        TC::testStackSize<   52 * K>();
        TC::testStackSize<   56 * K>();
        TC::testStackSize<   58 * K>();
        TC::testStackSize<   60 * K>();
        TC::testStackSize<   62 * K>();
        TC::testStackSize<   63 * K>();
        TC::testStackSize<   64 * K>();
        TC::testStackSize<   65 * K>();
        TC::testStackSize<   68 * K>();
        TC::testStackSize<   72 * K>();
        TC::testStackSize<   76 * K>();
        TC::testStackSize<   80 * K>();
        TC::testStackSize<   84 * K>();
        TC::testStackSize<   88 * K>();
        TC::testStackSize<   92 * K>();
        TC::testStackSize<   96 * K>();
        TC::testStackSize<  100 * K>();
        TC::testStackSize<  104 * K>();
        TC::testStackSize<  108 * K>();
        TC::testStackSize<  112 * K>();
        TC::testStackSize<  116 * K>();
        TC::testStackSize<  120 * K>();
        TC::testStackSize<  124 * K>();
        TC::testStackSize<  127 * K>();
        TC::testStackSize<  128 * K>();
        TC::testStackSize<  129 * K>();
        TC::testStackSize<  255 * K>();
        TC::testStackSize<  256 * K>();
        TC::testStackSize<  257 * K>();
        TC::testStackSize<  511 * K>();
        TC::testStackSize<  512 * K>();
        TC::testStackSize<  513 * K>();
        TC::testStackSize< 1023 * K>();
        TC::testStackSize< 1024 * K>();
        TC::testStackSize< 1025 * K>();
        TC::testStackSize< 2047 * K>();
        TC::testStackSize< 2048 * K>();
        TC::testStackSize< 2049 * K>();
        TC::testStackSize< 4095 * K>();
        TC::testStackSize< 4096 * K>();
        TC::testStackSize< 4097 * K>();
        TC::testStackSize< 8191 * K>();
        TC::testStackSize< 8192 * K>();
        TC::testStackSize< 8193 * K>();
        TC::testStackSize<16383 * K>();
        TC::testStackSize<16384 * K>();
        TC::testStackSize<16385 * K>();
      }  break;
      case 7: {
        // --------------------------------------------------------------------
        // DELETEKEY, THREAD-SPECIFICITY OF DATA TEST ON TLS
        //
        // Concerns:
        //   That 'deleteKey' deletes a TLS key for ALL threads, that data
        //   associated with a key by 'setSpecific' is thread-specific.
        //
        // Plan: Create 2 keys in the parent thread.  Verify both
        // exist in the child thread -- delete 1 in the child thread,
        // verify the data set in one thread is not visible from
        // another thread and does not affect the data set in another
        // thread.
        // --------------------------------------------------------------------

        if (verbose) cout <<
                       "'deleteKey', THREAD SPECIFICITY OF DATA IN TLS TEST\n"
                       "===================================================\n";

        namespace TC = BCEMT_THREADUTIL_TLSKEY_TEST6;

        Obj::createKey(&TC::parentKey1, &TlsDestructor6_1);
        Obj::createKey(&TC::parentKey2, &TlsDestructor6_2);

        int rc;
        rc = Obj::setSpecific(TC::parentKey1, (void *) 1);
        ASSERT(0 == rc);
        rc = Obj::setSpecific(TC::parentKey2, (void *) 2);
        ASSERT(0 == rc);

        bdlqq::ThreadUtil::Handle handle;
        bdlqq::ThreadUtil::create(&handle, TC::TlsKeyTestFunctor(10));
        bdlqq::ThreadUtil::join(handle);

        bdlqq::ThreadUtil::create(&handle, TC::TlsKeyTestFunctor(20));
        bdlqq::ThreadUtil::join(handle);

        bdlqq::ThreadUtil::create(&handle, TC::TlsKeyTestFunctor(30));
        bdlqq::ThreadUtil::join(handle);

        rc = Obj::deleteKey(TC::parentKey1);
        ASSERT(0 == rc);
        rc = Obj::deleteKey(TC::parentKey1);
        ASSERT(0 != rc);

        ASSERT(0 == TC::terminations1);
        ASSERT(3 == TC::terminations2);

#if 0
        // parentKey1 is deleted -- this is undefined behavior

        ASSERT((void *) 1 == Obj::getSpecific(TC::parentKey1));
        ASSERT(0 == Obj::setSpecific(TC::parentKey1, (void *) 7));
#endif

        // check 'parentKey2; is unaffected
        ASSERT((void *) 2 == Obj::getSpecific(TC::parentKey2));
      }  break;
      case 6: {
        // --------------------------------------------------------------------
        // TESTING CREATEKEY, SETSPECIFIC, AND GETSPECIFIC
        //
        // Concerns: That the destructor function passed to 'createKey' is
        //   executed by the thread that is terminating, that 'createKey',
        //   'setSpecific', // and 'getSpecific' all work in the thread as well
        //   as in the destructor function.
        //
        // Plan:
        //   Spawn a thread, and within that thread create 2 keys and associate
        //   non-zero values with them, and both keys associated with the same
        //   destructor.  Verify that the destructor is called twice.  Repeat
        //   the process passing 0 in the 'destructor' fields of the keys, and
        //   verify that the destructor is not called.
        //       The second time the destructor is terminated, associate a
        //   non-zero value with one of the keys and verify this results in its
        //   being called again.
        // --------------------------------------------------------------------

        namespace TC = BCEMT_THREADUTIL_CREATEKEY_TEST5;

        Obj::Key parentKey;
        int rc = Obj::createKey(&parentKey, 0);
        ASSERT(0 == rc);

        void *data = Obj::getSpecific(parentKey);
        ASSERT(0 == data);

        rc = Obj::setSpecific(parentKey, (void *) 2);
        ASSERT(0 == rc);

        ASSERT((void *) 2 == Obj::getSpecific(parentKey));

        bdlqq::ThreadUtil::Handle handle;
        rc =
            bdlqq::ThreadUtil::create(&handle, TC::CreateKeyTestFunctor(true));
        ASSERT(0 == rc);
        bdlqq::ThreadUtil::join(handle);

#if defined(BDLQQ_PLATFORM_POSIX_THREADS) && !defined(BSLS_PLATFORM_OS_CYGWIN)
        ASSERT(3 == TC::terminated);
#else
        ASSERT(2 == TC::terminated);
#endif

        ASSERT(parentKey != TC::childKey1);
        ASSERT(Obj::selfId() != TC::childId);

        ASSERT((void *) 2 == Obj::getSpecific(parentKey));
        ASSERT(0 == Obj::getSpecific(TC::childKey1));

        TC::terminated = 0;
        TC::childKey1 = parentKey;
        TC::childId = Obj::selfId();

        bdlqq::ThreadUtil::create(&handle, TC::CreateKeyTestFunctor(false));
        bdlqq::ThreadUtil::join(handle);

        ASSERT(0 == TC::terminated);
        ASSERT(parentKey != TC::childKey1);
        ASSERT(Obj::selfId() != TC::childId);

        ASSERT((void *) 2 == Obj::getSpecific(parentKey));
        ASSERT(0 == Obj::getSpecific(TC::childKey1));
        ASSERT(0 == Obj::getSpecific(TC::childKey2));
      }  break;
      case 5: {
        // --------------------------------------------------------------------
        // TESTING MICROSLEEP
        //
        // Concerns:
        //   That 'microSleep' always sleeps at least the given amount of
        //   time, and does not sleep an unreasonable amount of extra time.
        //
        // Plan:
        //   Several times, sleep for 0.3 seconds, then check how much time
        //   has passed and verify that it is within acceptable boundaries.
        // --------------------------------------------------------------------

        enum { SLEEP_MICROSECONDS = 300 * 1000 };
        const double SLEEP_SECONDS = SLEEP_MICROSECONDS * 1e-6;
        const double OVERSHOOT_MIN = -1e-5;
#if defined(BSLS_PLATFORM_OS_SOLARIS) || defined(BSLS_PLATFORM_OS_LINUX)
            const double TOLERANCE = 2;   // microSleep is obscenely
                                          // imprecise on Sun and to a
                                          // lesser extent Linux
#else
            const double TOLERANCE = 0.05;
#endif

        for (int i = 0; i < 8; ++i) {
            double start   = bdlt::CurrentTime::now().totalSecondsAsDouble();
            bdlqq::ThreadUtil::microSleep(SLEEP_MICROSECONDS);
            double elapsed = bdlt::CurrentTime::now().totalSecondsAsDouble() -
                                                                         start;

            double overshoot = elapsed - SLEEP_SECONDS;

            if (veryVerbose) P(overshoot);

            LOOP_ASSERT( overshoot, overshoot >= OVERSHOOT_MIN);
            LOOP2_ASSERT(overshoot, TOLERANCE, overshoot < TOLERANCE);
        }
    }  break;
    case 4: {
        // --------------------------------------------------------------------
        // TESTING USAGE Example 2
        // --------------------------------------------------------------------

        if (verbose) cout << "\nSmall stack usage example" << endl;

        createSmallStackSizeThread();    // usage example 2
    }  break;
    case 3: {
        // --------------------------------------------------------------------
        // TESTING USAGE Example 1
        //
        // Concern: that the usage examples (including those that were
        // previously in the 'bdlqq_thread' component) compile and work
        // properly.
        // --------------------------------------------------------------------

        // BASIC EXAMPLE
        if (verbose) cout << "\nBasic thread utilities example" << endl;

        bdlqq::Configuration::setDefaultThreadStackSize(
                    bdlqq::Configuration::recommendedDefaultThreadStackSize());

        bdlqq::ThreadAttributes attr;
        attr.setStackSize(1024 * 1024);

        bdlqq::ThreadUtil::Handle handle;
        int rc = bdlqq::ThreadUtil::create(&handle, attr, myThreadFunction, 0);
        ASSERT(0 == rc);
        bdlqq::ThreadUtil::yield();
        rc = bdlqq::ThreadUtil::join(handle);
        ASSERT(0 == rc);

        if (verbose) bsl::cout << "A three second interval has elapsed\n";
    }  break;
    case 2: {
        // --------------------------------------------------------------------
        // TESTING 'isEqual'
        //
        // Concerns: A valid thread handle is equal to itself.  Two valid
        // thread handles are equal.  An invalid thread handle is unequal to
        // any other thread handle, including itself.
        //
        // --------------------------------------------------------------------
        if (verbose) {
            cout << "\n isEqual test" << endl;
        }

        bdlqq::ThreadUtil::Handle validH1, validH2;
        bdlqq::ThreadUtil::create(&validH1, myThreadFunction, 0);
        bdlqq::ThreadUtil::create(&validH2, myThreadFunction, 0);
        bdlqq::ThreadUtil::Handle validH1copy = validH1;

        ASSERT(1 == bdlqq::ThreadUtil::isEqual(validH1, validH1));
        ASSERT(1 == bdlqq::ThreadUtil::isEqual(validH1, validH1copy));
        ASSERT(0 == bdlqq::ThreadUtil::isEqual(validH1, validH2));
        ASSERT(0 == bdlqq::ThreadUtil::isEqual(
                                            bdlqq::ThreadUtil::invalidHandle(),
                                            validH2));
        ASSERT(0 == bdlqq::ThreadUtil::isEqual(
                                            bdlqq::ThreadUtil::invalidHandle(),
                                            validH1copy));
        ASSERT(0 == bdlqq::ThreadUtil::isEqual(validH1,
                                          bdlqq::ThreadUtil::invalidHandle()));
        ASSERT(1 == bdlqq::ThreadUtil::isEqual(
                                          bdlqq::ThreadUtil::invalidHandle(),
                                          bdlqq::ThreadUtil::invalidHandle()));

        bdlqq::ThreadUtil::join(validH1);
        bdlqq::ThreadUtil::join(validH2);
    }  break;
    case 1: {
        // --------------------------------------------------------------------
        // Invokable functor test
        // --------------------------------------------------------------------

       if (verbose) {
          cout << "\nInvokable functor test" << endl;
       }

       enum { THREAD_COUNT = 10 }; // Actually twice this many

       bdlqq::ThreadAttributes detached;
       detached.setDetachedState(
                               bdlqq::ThreadAttributes::e_CREATE_DETACHED);

       ThreadChecker joinableChecker;
       ThreadChecker detachedChecker;
       bdlqq::ThreadUtil::Handle handles[THREAD_COUNT], dummy;
       for (int i = 0; i < THREAD_COUNT; ++i) {
          ASSERT(0 == bdlqq::ThreadUtil::create(&handles[i],
                                               joinableChecker.getFunctor()));
          ASSERT(0 == bdlqq::ThreadUtil::create(&dummy,
                                               detached,
                                               detachedChecker.getFunctor()));
       }

       // Join the joinable threads
       for (int i = 0; i < THREAD_COUNT; ++i) {
          ASSERT(0 == bdlqq::ThreadUtil::join(handles[i]));
       }

       int iterations = 100;
       while ((THREAD_COUNT != joinableChecker.count() ||
               THREAD_COUNT != detachedChecker.count()) &&
              0 < --iterations)
       {
           bdlqq::ThreadUtil::microSleep(100 * 1000);  // 100 msec
           bdlqq::ThreadUtil::yield();
       }

       ASSERT(THREAD_COUNT == joinableChecker.count());
       ASSERT(THREAD_COUNT == detachedChecker.count());
      }  break;
      case -1: {
        // --------------------------------------------------------------------
        // STACK OVERFLOW TEST
        //
        // Concern:
        //   How big is the stack, really?  The main concern here is that the
        //   stack size on HPUX is being properly adjusted so that the
        //   specified stack size really is close to the effective stack size.
        //
        // Plan:
        //   Recurse, printing out how deep we are, until we overflow.
        //
        // Observations:
        //   So: Solaris
        //   AI: AIX
        //   HP: HPUX
        //   Li: Linux
        //   Wi: Windows
        //
        //   Results accurate to 2% or worse
        //
        //   Note that on AIX, the stack overflow causes the thread to die
        //   without any warning messages, and the thread attempting to join
        //   it just hangs.  On other Unix platforms, the stack overflow causes
        //   a segfault.
        //
        //   Native Limit:
        //   -------------
        //
        //   So 32:  1035871
        //   So 64:  2082903
        //
        //   AI 32:   112128
        //   AI 64:   213760
        //
        //   HP 32:   110240
        //   HP 64:   110240
        //
        //   Li 32: 66691239
        //   Li 64: 66359287
        //
        //   Wi 32:  1022207
        //
        //   export CASE_MINUS_1_STACK_SIZE=1000000:
        //   ---------------------------------------
        //
        //   So 32:   994111
        //   So 64:  1005879
        //
        //   AI 32:  1020832
        //   AI 64:  1118720
        //
        //   HP 32:   966160
        //   HP 64:   966160
        //
        //   Li 32:   976567
        //   Li 64:   991159
        //
        //   Wi 32:  1022207
        //
        //   export CASE_MINUS_1_DEFAULT_SIZE=800000:
        //   ----------------------------------------
        //
        //   So 32:   789119
        //   So 64:   787479
        //
        //   AI 32:   884176
        //   AI 64:   806400
        //
        //   HP 32:   771680
        //   HP 64:   771680
        //
        //   Li 32:   780391
        //   Li 64:   775735
        //
        //   Wi 32:   834239
        // --------------------------------------------------------------------

        bdlqq::ThreadAttributes attr;

        const char *stackSizeString = bsl::getenv("CASE_MINUS_1_STACK_SIZE");
        if (stackSizeString) {
            attr.setStackSize(atoi(stackSizeString));
            P(attr.stackSize());
        }

        const char *defaultSizeString =
                                      bsl::getenv("CASE_MINUS_1_DEFAULT_SIZE");
        if (defaultSizeString) {
            BSLS_ASSERT_OPT(!stackSizeString);
            bdlqq::Configuration::setDefaultThreadStackSize(
                                                      atoi(defaultSizeString));
            P(bdlqq::Configuration::defaultThreadStackSize());
        }

        bdlqq::ThreadUtil::Handle handle;
        if (stackSizeString) {
            bdlqq::ThreadUtil::create(&handle, attr, &testCaseMinus1ThreadMain,
                                     0);
        }
        else {
            bdlqq::ThreadUtil::create(&handle, &testCaseMinus1ThreadMain, 0);
        }

        bdlqq::ThreadUtil::join(handle);
      }  break;
#ifndef BSLS_PLATFORM_OS_WINDOWS
      case -2: {
        // --------------------------------------------------------------------
        // CLEARANCE TEST
        //
        // Concern:
        //   What is the minimum amount of stack needed for a thread to
        //   function?
        //
        // Plan:
        //   Specify a stack size at run time and verify that threads can be
        //   spawned with that stack size.  Since this test uses 'alloca.h',
        //   which doesn't exist on Windows, the test is disabled there.
        // --------------------------------------------------------------------

        bdlqq::ThreadAttributes attr;
        ASSERT(verbose);
        int clearanceTestStackSize = bsl::atoi(argv[2]);
        P(clearanceTestStackSize);
        attr.setStackSize(clearanceTestStackSize);
        attr.setGuardSize(MIN_GUARD_SIZE);

        clearanceTestAllocaSize = 0;
        clearanceTestState = CLEARANCE_TEST_START;
        bdlqq::ThreadUtil::Handle handle;
        int rc = bdlqq::ThreadUtil::create(&handle, attr, &clearanceTest, 0);
        ASSERT(0 == rc);
        rc = bdlqq::ThreadUtil::join(handle);
        ASSERT(0 == rc);
        ASSERT(CLEARANCE_TEST_DONE == clearanceTestState);

        Q(Test 0 Completed);

        for (clearanceTestAllocaSize = clearanceTestStackSize / 2;;
                                              clearanceTestAllocaSize += 100) {
            int diff = clearanceTestStackSize - clearanceTestAllocaSize;
            P(diff);

            clearanceTestState = CLEARANCE_TEST_START;
            rc = bdlqq::ThreadUtil::create(&handle, attr, &clearanceTest, 0);
            ASSERT(0 == rc);
            rc = bdlqq::ThreadUtil::join(handle);
            ASSERT(0 == rc);

            ASSERT(CLEARANCE_TEST_DONE == clearanceTestState);
        }

        Q(Alloca Test Completed);
      }  break;
#endif
      case -3: {
        // --------------------------------------------------------------------
        // STACK GROWTH DIRECTION TEST
        //
        // Concern:
        //   Determine whether the stack grows in a positive or negative
        //   direction.
        //
        // Plan:
        //   Declare two automatic variables in two different stack frames,
        //   subtract pointers between the two to deterimine direction of stack
        //   growth.
        // --------------------------------------------------------------------

        char c;

        cout << (stackGrowthIsNegative(&c) ? "negative" : "positive") << endl;
      }  break;
      case -4: {
        // --------------------------------------------------------------------
        // SECOND STACK CLEARANCE TEST
        //
        // Concern:
        //   Determine stack size by, rather than recursing, just accessing
        //   memory further and further up the stack.
        // --------------------------------------------------------------------

        setbuf(stdout, 0);

        bdlqq::ThreadAttributes attr;
        ASSERT(verbose);

#ifdef PTHREAD_STACK_MIN
        int stackSize = (int) PTHREAD_STACK_MIN;
#else
        int stackSize = 1 << 17;
#endif
        if (verbose) {
            stackSize += atoi(argv[2]);
        }
        printf("stackSize = %d\n", stackSize);

        attr.setStackSize(stackSize);
        attr.setGuardSize(MIN_GUARD_SIZE);

        bdlqq::ThreadUtil::Handle handle;
        int rc = bdlqq::ThreadUtil::create(&handle,
                                          attr,
                                          &secondClearanceTest,
                                          (void *) stackSize);
        ASSERT(0 == rc);
        rc = bdlqq::ThreadUtil::join(handle);
      }  break;
      case -5: {
        // --------------------------------------------------------------------
        // TESTING: sleepUntil (Longer duration)
        //
        // Note that this test case is intended to be run manually, and is an
        // extension to test case 2 that tests for longer durations than should
        // be run in a typical build cycle.
        //
        // Concerns:
        //: 1 'sleepUntil' suspends the current thread until the indicated
        //:    time in the future (within some reasonable limit).  For
        //:    times > 1s in the future.
        //
        // Plan:
        //: 1 Call 'sleepUntil' for a series of values less than a second in
        //:   the future, and verify that system time after sleeping is within
        //:   a reasonable range of the expected target time. (C-1)
        //
        // Testing:
        //   void sleepUntil(const bsls::TimeInterval& );
        // --------------------------------------------------------------------

        if (verbose) {
            cout << endl
                 << "CLASS METHOD 'sleepUntil'" << endl
                 << "=========================" << endl;
        }

        if (veryVerbose) {
            cout << "sleepUntil for times in the future" << endl;
        }

        for (int i = 0; i < 5; ++i) {
            if (veryVeryVerbose) {
                cout << "sleepUntil for " << i * 30 << "s" << endl;
            }

            bsls::TimeInterval expectedTime = bdlt::CurrentTime::now();

            expectedTime.addSeconds(i * 30);

            Obj::sleepUntil(expectedTime);

            bsls::TimeInterval actualTime = bdlt::CurrentTime::now();

            ASSERT(actualTime >= expectedTime);
            LOOP_ASSERT((actualTime - expectedTime).totalMilliseconds(),
                        (actualTime - expectedTime).totalMilliseconds() < 50);
        }

    }  break;
      default: {
        cerr << "WARNING: CASE `" << test << "' NOT FOUND." << endl;
        testStatus = -1;
      }
    }

    if (testStatus > 0) {
        cerr << "Error, non-zero test status = "
             << testStatus << "." << endl;
    }
    return testStatus;
}

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
