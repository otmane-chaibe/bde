// btes5_networkconnector.t.cpp                                       -*-C++-*-

#include <btes5_networkconnector.h>
#include <btes5_testserver.h> // for testing only

#include <bcec_fixedqueue.h>
#include <bcema_testallocator.h>            // thread-safe allocator
#include <bcemt_threadutil.h>               // for sleep
#include <bdef_bind.h>
#include <bdef_placeholder.h>
#include <bsl_iostream.h>
#include <bslma_defaultallocatorguard.h>
#include <bslma_default.h>
#include <bslma_testallocator.h>
#include <bsls_assert.h>
#include <bsls_asserttest.h>
#include <bsl_sstream.h>
#include <btemt_session.h>                  // for testing only
#include <btemt_sessionpool.h>              // for testing only
#include <bteso_inetstreamsocketfactory.h>

#include <bsl_cerrno.h>

using namespace BloombergLP;
using namespace bsl;

//=============================================================================
//                             TEST PLAN
//-----------------------------------------------------------------------------
//                              Overview
//                              --------
// This component implements connection establishment through a network of
// SOCKS5 proxies. In order to test it, 'btes5_TestServer' is used to implement
// a SOCKS5 prixy. In some tests, more than one test servers are connected
// (cascaded) to test connection through more than one level of proxy. A test
// server is also used to test failure scenarios such as timeout and failover.
//-----------------------------------------------------------------------------
// CREATORS
// [2] btes5_NetworkConnector(net, fact, eventMgr, *a = 0);
// [ ] btes5_NetworkConnector(net, fact, eventMgr, min, max, *a = 0);
// [ ] btes5_NetworkConnector(net, fact, eventMgr, prov, *a = 0);
// [ ] btes5_NetworkConnector(net, fact, eventMgr, min, max, prov, *a = 0);
// [2] ~btes5_NetworkConnector();
// MANIPULATORS
// [2] AttemptHandle makeAttemptHandle(cB, proxyTO, totalTO, server);
// [2] void startAttempt(AttemptHandle& connectionAttempt);
// [ ] void cancelAttempt(AttemptHandle& connectionAttempt);
//-----------------------------------------------------------------------------
// [1] BREATHING TEST
// [8] USAGE EXAMPLE
// [2] CONCERN: All memory allocation is from the object's allocator.
// [3] CONCERN: Timeout when the proxy doesn't respond.
// [3] CONCERN: Failover to second proxy after the first proxy timeout.
// [4] CONCERN: Failover to second proxy after the first proxy failure.
// [ ] CONCERN: Failover to second proxy after the first proxy doesn't exist.
// [5] CONCERN: Failure when the only proxy can't be resolved.
// [ ] CONCERN: Failure when the only proxy fails (destination unreachable).
// [8] CONCERN: Normal connection with proxy and destination as hostname.
// [ ] CONCERN: Normal connection with proxy as IP and destination as hostname.
// [ ] CONCERN: Normal connection with proxy and destination as IP.
// [6] CONCERN: Two overlapping connection attempts on two connectors.
// [-2] CONCERN: Two overlapping connection attempts on one connector.
// [3] CONCERN: Total timeout before proxy timeout.
// [4] CONCERN: Connection with 2 levels.
// [4] CONCERN: Two proxy levels, one proxy in each level failing.
// [7] CONCERN: Connection with 3 levels.
// [ ] CONCERN: Connection with 3 levels, with 2 proxies in each level failing.
// [ ] CONCERN: Cancelling a connection attempt.
// [4] CONCERN: Connect with construction-time username/password.
// [ ] CONCERN: Connect with credentials provider.
// [2] CONCERN: Import connected socket into 'btemt_SessionPool'.
// [-1] CONCERN: Negotiate using an off-the-shelf SOCKS5 proxy.


// ============================================================================
//                    STANDARD BDE ASSERT TEST MACROS
// ----------------------------------------------------------------------------

static int testStatus = 0;

static void aSsErT(int c, const char *s, int i)
{
    if (c) {
        cout << "Error " << __FILE__ << "(" << i << "): " << s
             << "    (failed)" << endl;
        if (testStatus >= 0 && testStatus <= 100) ++testStatus;
    }
}
# define ASSERT(X) { aSsErT(!(X), #X, __LINE__); }

// ============================================================================
//                  STANDARD BDE LOOP-ASSERT TEST MACROS
// ----------------------------------------------------------------------------

#define LOOP_ASSERT(I,X) {                                                    \
    if (!(X)) { cout << #I << ": " << I << "\n"; aSsErT(1, #X, __LINE__);}}

#define LOOP2_ASSERT(I,J,X) {                                                 \
    if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": "                 \
              << J << "\n"; aSsErT(1, #X, __LINE__); } }

#define LOOP3_ASSERT(I,J,K,X) {                                               \
   if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": " << J << "\t"     \
              << #K << ": " << K << "\n"; aSsErT(1, #X, __LINE__); } }

#define LOOP4_ASSERT(I,J,K,L,X) {                                             \
   if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": " << J << "\t" <<  \
       #K << ": " << K << "\t" << #L << ": " << L << "\n";                    \
       aSsErT(1, #X, __LINE__); } }

#define LOOP5_ASSERT(I,J,K,L,M,X) {                                           \
   if (!(X)) { cout << #I << ": " << I << "\t" << #J << ": " << J << "\t" <<  \
       #K << ": " << K << "\t" << #L << ": " << L << "\t" <<                  \
       #M << ": " << M << "\n";                                               \
       aSsErT(1, #X, __LINE__); } }

// ============================================================================
//                  SEMI-STANDARD TEST OUTPUT MACROS
// ----------------------------------------------------------------------------

#define P(X) cout << #X " = " << (X) << endl; // Print identifier and value.
#define Q(X) cout << "<| " #X " |>" << endl;  // Quote identifier literally.
#define P_(X) cout << #X " = " << (X) << ", " << flush; // 'P(X)' without '\n'
#define T_ cout << "\t" << flush;             // Print tab w/o newline.
#define L_ __LINE__                           // current Line number

// ============================================================================
//                  NEGATIVE-TEST MACRO ABBREVIATIONS
// ----------------------------------------------------------------------------

#define ASSERT_SAFE_PASS(EXPR) BSLS_ASSERTTEST_ASSERT_SAFE_PASS(EXPR)
#define ASSERT_SAFE_FAIL(EXPR) BSLS_ASSERTTEST_ASSERT_SAFE_FAIL(EXPR)
#define ASSERT_PASS(EXPR)      BSLS_ASSERTTEST_ASSERT_PASS(EXPR)
#define ASSERT_FAIL(EXPR)      BSLS_ASSERTTEST_ASSERT_FAIL(EXPR)
#define ASSERT_OPT_PASS(EXPR)  BSLS_ASSERTTEST_ASSERT_OPT_PASS(EXPR)
#define ASSERT_OPT_FAIL(EXPR)  BSLS_ASSERTTEST_ASSERT_OPT_FAIL(EXPR)

// ============================================================================
//                 GLOBAL TYPEDEFS AND VARIABLES FOR TESTING
// ----------------------------------------------------------------------------

namespace {

bool             verbose = 0;
bool         veryVerbose = 0;
bool     veryVeryVerbose = 0;
bool veryVeryVeryVerbose = 0;

// ============================================================================
//                     CLASSES AND FUNCTIONS USED IN TESTS
// ----------------------------------------------------------------------------
void breathingTestCb(
    btes5_NetworkConnector::ConnectionStatus                   status,
    bteso_StreamSocket<bteso_IPv4Address>                     *socket,
    bteso_StreamSocketFactory<bteso_IPv4Address>              *socketFactory,
    const btes5_DetailedError&                                 error,
    bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> *queue)
    // Process a SOCKS5 connection response with the specified 'status' and
    // 'error', with the specified 'socket' allocated by the specified
    // 'socketFactory', and append 'status' to the specified 'queue'.
{
    if (verbose) {
        cout << "Connection attempt concluded with"
             << " status=" << status << " error=" << error << endl;
    }
    if (btes5_NetworkConnector::e_SUCCESS == status) {
        socketFactory->deallocate(socket);
    }
    queue->pushBack(status);
}

// ============================================================================
//                     btemt_SessionPool testing callback
// ----------------------------------------------------------------------------
void poolStateCb(int state, int source, void *)
{
    if (verbose) {
        cout << "Session Pool: state=" << state
             << " source=" << source
             << endl;
    }
}

void sessionStateCb(int            state,
                    int            ,
                    btemt_Session *,
                    void          *)
{
    if (veryVerbose) { T_ P(state) }
}

struct Session : public btemt_Session {
  public:
    virtual int start()
    {
        if (veryVerbose) { cout << "Session::start" << endl; }
        return 0;
    }

    virtual int stop()
    {
        if (veryVerbose) { cout << "Session::stop" << endl; }
        return 0;
    }

    virtual btemt_AsyncChannel *channel() const
    {
        return 0;
    }

    virtual ~Session()
    {
    }
};

class SessionFactory : public btemt_SessionFactory {
  public:
    virtual void allocate(btemt_AsyncChannel *, const Callback& cb)
    {
        cb(0, new Session);
        if (veryVerbose) { cout << "SessionFactory::allocate" << endl; }
    }

    virtual void deallocate(btemt_Session *session)
    {
        delete dynamic_cast<Session*>(session);
        if (veryVerbose) { cout << "SessionFactory::deallocate" << endl; }
    }
};

void socks5Cb(btes5_NetworkConnector::ConnectionStatus      status,
              bteso_StreamSocket<bteso_IPv4Address>        *socket,
              bteso_StreamSocketFactory<bteso_IPv4Address> *socketFactory,
              const btes5_DetailedError&                    error,
              btemt_SessionPool                            *sessionPool,
              bcemt_Mutex                                  *stateLock,
              bcemt_Condition                              *stateChanged,
              volatile int                                 *state)
{
    bcemt_LockGuard<bcemt_Mutex> lock(stateLock);
    if (status == btes5_NetworkConnector::e_SUCCESS) {
        *state = 1;
        if (verbose) cout << "connection succeeded" << endl;

        SessionFactory sessionFactory;
        int handle;
        using namespace bdef_PlaceHolders;
        int rc = sessionPool->import(&handle,
                                     bdef_BindUtil::bind(sessionStateCb,
                                                         _1, _2, _3, _4),
                                     socket,
                                     socketFactory,
                                     &sessionFactory);

        if (verbose) cout << "btemt_SessionPool::import rc=" << rc << endl;
        *state = rc ? -1 : 1;  // 'rc == 0' is success
        sessionPool->stop();
    } else {
        *state = -1;
        cout << "Connect failed " << status << ": " << error << endl;
    }
    stateChanged->signal();
}

// ============================================================================
//                  USAGE EXAMPLE
// ----------------------------------------------------------------------------
///Example 1: Connect to a server through two proxy levels
///- - - - - - - - - - - - - - - - - - - - - - - - - - - -
// We want to connect to a server reachable through two levels of proxies:
// first through one of corporate SOCKS5 servers, and then through one of
// regional SOCKS5 servers.
//
// First we define a callback function to process connection status, and if
// successful perform useful work and finally deallocate the socket. After the
// work is done (or error is reported) we signal the main thread with the
// status; this also signifies that we no longer need the stream factory passed
// to us.
//..
    void connectCb(int                                           status,
                   bteso_StreamSocket< bteso_IPv4Address>       *socket,
                   bteso_StreamSocketFactory<bteso_IPv4Address> *socketFactory,
                   const btes5_DetailedError&                    error,
                   bcemt_Mutex                                  *stateLock,
                   bcemt_Condition                              *stateChanged,
                   volatile int                                 *state)
    {
        if (0 == status) {
            // Success: conduct I/O operations with 'socket' ... and deallocate
            socketFactory->deallocate(socket);
        } else {
            cout << "Connect failed " << status << ": " << error << endl;
        }
        bcemt_LockGuard<bcemt_Mutex> lock(stateLock);
        *state = status ? -1 : 1; // 1 for success, -1 for failure
        stateChanged->signal();
    }
//..
// Then we define the level of proxies that should be reachable directory.
//..
    static int connectThroughProxies(const bteso_Endpoint& corpProxy1,
                                     const bteso_Endpoint& corpProxy2)
    {
        btes5_NetworkDescription proxies;
        proxies.addProxy(0, corpProxy1);
        proxies.addProxy(0, corpProxy2);
//..
// Next we add a level for regional proxies reachable from the corporate
// proxies. Note that .tk stands for Tokelau in the Pacific Ocean.
//..
        proxies.addProxy(1, bteso_Endpoint("proxy1.example.tk", 1080));
        proxies.addProxy(1, bteso_Endpoint("proxy2.example.tk", 1080));
//..
// Then we set the user name and password which will be used in case one of the
// proxies in the connection path requires that type of authentication.
//..
        btes5_Credentials credentials("John.smith", "pass1");
        btes5_NetworkDescriptionUtil::setAllCredentials(&proxies, credentials);
//..
// Now we construct a 'btes5_NetworkConnector' which will be used to connect
// to one or more destinations.
//..
        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();
        btes5_NetworkConnector connector(proxies, &factory, &eventManager);
//..
// Finally we attempt to connect to the destination. Input, output and eventual
// closing of the connection will be handled from 'connectCb', which will
// signal the using 'state', with the access protected by a mutex and condition
// variable.
//..
        const bdet_TimeInterval proxyTimeout(5.0);
        const bdet_TimeInterval totalTimeout(30.0);
        bcemt_Mutex     stateLock;
        bcemt_Condition stateChanged;
        volatile int    state = 0; // value > 0 indicates success, < 0 is error
        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::AttemptHandle attempt
            = connector.makeAttemptHandle(bdef_BindUtil::bind(connectCb,
                                                              _1, _2, _3, _4,
                                                              &stateLock,
                                                              &stateChanged,
                                                              &state),
                          proxyTimeout,
                          totalTimeout,
                          bteso_Endpoint("destination.example.com", 8194));
        connector.startAttempt(attempt);
        bcemt_LockGuard<bcemt_Mutex> lock(&stateLock);
        while (!state) {
            stateChanged.wait(&stateLock);
        }
        return state;
    }
//..

}  // close unnamed namespace

// ============================================================================
//                            MAIN PROGRAM
// ----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    int test = argc > 1 ? atoi(argv[1]) : 0;

                verbose = argc > 2;
            veryVerbose = argc > 3;
        veryVeryVerbose = argc > 4;
    veryVeryVeryVerbose = argc > 5;

    const btes5_TestServerArgs::Severity verbosity =
        veryVeryVerbose ? btes5_TestServerArgs::e_TRACE
                        : veryVerbose
                        ? btes5_TestServerArgs::e_DEBUG
                        : verbose
                        ? btes5_TestServerArgs::e_ERROR
                        : btes5_TestServerArgs::e_NONE;

    // preserve allocator for test apparatus

    bslma::Allocator *alloc = bslma::Default::defaultAllocator();

    cout << "TEST " << __FILE__ << " CASE " << test << endl;

    switch (test) { case 0:
      case 8: {
        // --------------------------------------------------------------------
        // USAGE EXAMPLE
        //
        // Concerns:
        //: 1 The usage example compiles and can be executed successfully.
        //
        // Plan:
        //: 1 Create 2 levels of proxies configured to simulate connection.
        //: 2 Uncomment the usage example.
        //: 3 Verify successful compilation and execution.
        //
        // Testing:
        //   USAGE EXAMPLE
        //   CONCERN: Normal connection with proxy and destination as hostname.
        //   CONCERN: Connection with 2 levels.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "USAGE EXAMPLE" << endl
                          << "=============" << endl;

        bcema_TestAllocator da("defaultAllocator", veryVeryVerbose);
        bslma::DefaultAllocatorGuard guard(&da);

        bteso_Endpoint destination;
        btes5_TestServerArgs destinationArgs;
        destinationArgs.d_verbosity = verbosity;
        destinationArgs.d_label = "destination";
        destinationArgs.d_mode = btes5_TestServerArgs::e_IGNORE;
        btes5_TestServer destinationServer(&destination, &destinationArgs);

        bteso_Endpoint region;
        btes5_TestServerArgs regionArgs;
        regionArgs.d_verbosity = verbosity;
        regionArgs.d_label = "regionProxy";
        regionArgs.d_mode = btes5_TestServerArgs::e_CONNECT;
        regionArgs.d_expectedDestination.set("destination.example.com", 8194);
        regionArgs.d_destination = destination; // override connection address
        btes5_TestServer regionServer(&region, &regionArgs);

        btes5_TestServerArgs corpArgs;
        corpArgs.d_verbosity = verbosity;
        corpArgs.d_label = "corpProxy";
        corpArgs.d_destination = region; // override connection address
        corpArgs.d_mode = btes5_TestServerArgs::e_CONNECT;
        bteso_Endpoint proxy;
        btes5_TestServer proxyServer(&proxy, &corpArgs);

        ASSERT(connectThroughProxies(proxy, proxy) > 0);
      } break;
      case 7: {
        // --------------------------------------------------------------------
        // 3-LEVEL CASCADED PROXIES
        //  Test of connectivity using 3 levels of proxies.
        //
        // Concerns:
        //: 1 Connection with 3 levels of proxies.
        //
        // Plan:
        //: 1 Using a 'btes5_TestServer' construct a destination server.
        //: 2 Make a 3-level proxy network with some "bad" proxies:
        //:   1 One unresponsive proxy in level 0
        //:   2 One proxy with non-matching credentials in level 1
        //: 3 Verify successful connection to destination.
        //
        // Testing:
        //   CONCERN: Connection with 3 levels.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "3-LEVEL CASCADED PROXIES" << endl
                          << "========================" << endl;

        btes5_TestServerArgs args;  // reused for all servers
        args.d_verbosity = verbosity;
        const btes5_Credentials credentials("gooduser", "goodpass");

        bteso_Endpoint destination;
        args.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &args);

        bteso_Endpoint proxyAddress;  // address of just-started proxy server
        btes5_NetworkDescription proxies;

        args.d_label = "unresponsive-0.0";
        args.d_mode = btes5_TestServerArgs::e_IGNORE;
        btes5_TestServer proxy00(&proxyAddress, &args);
        proxies.addProxy(0, proxyAddress, credentials);

        args.d_label = "proxy-0.1";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        btes5_TestServer proxy01(&proxyAddress, &args);
        proxies.addProxy(0, proxyAddress, credentials);

        args.d_label = "proxy-0.2";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        btes5_TestServer proxy02(&proxyAddress, &args);
        proxies.addProxy(0, proxyAddress, credentials);

        args.d_label = "bad-credentials-1.0";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        args.d_expectedCredentials.set("baduser", "badpass");
        btes5_TestServer proxy10(&proxyAddress, &args);
        proxies.addProxy(1, proxyAddress, credentials);

        args.d_label = "proxy-1.1";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        args.d_expectedCredentials.set("gooduser", "goodpass");
        btes5_TestServer proxy11(&proxyAddress, &args);
        proxies.addProxy(1, proxyAddress, credentials);

        args.d_label = "proxy-2.0";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        args.d_expectedCredentials.set("gooduser", "goodpass");
        btes5_TestServer proxy20(&proxyAddress, &args);
        proxies.addProxy(2, proxyAddress, credentials);

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> queue(1);
        btes5_NetworkConnector::ConnectionStatus status;

        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::ConnectionStateCallback
            cb = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4, &queue);

        if (veryVerbose) { P_(destination) P(proxies) }

        btes5_NetworkConnector connector(proxies, &factory, &eventManager);
        btes5_NetworkConnector::AttemptHandle attempt
            = connector.makeAttemptHandle(cb,
                                          bdet_TimeInterval(0.5), // proxy
                                          bdet_TimeInterval(5.0), // total
                                          destination);
        connector.startAttempt(attempt);
        queue.popFront(&status);
        LOOP_ASSERT(status, btes5_NetworkConnector::e_SUCCESS == status);
      } break;
      case 6: {
        // --------------------------------------------------------------------
        // OVERLAPPED CONNECTIONS
        //  Test concurrency through overlapping conection attempts.
        //
        // Concerns:
        //: 1 Two connections can be established concurrently, using different
        //:   connectors and different proxies.
        //
        // Plan:
        //: 1 Using a table-driven method:
        //:   1 Start two test proxies with a given response-delays
        //:   1 Construct and start a 'btes5_NetworkConnector' using one proxy
        //:   2 After a delay, start the second connector via the other proxy
        //:   3 Check for successful connection using the two connectors.
        //
        // Testing:
        //   CONCERN: Two overlapping connection attempts on two connectors.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "OVERLAPPED CONNECTIONS" << endl
                          << "======================" << endl;

        bcema_TestAllocator defaultAllocator("default", veryVeryVeryVerbose);
        bslma::Default::setDefaultAllocator(&defaultAllocator);

        btes5_TestServerArgs args;  // reused for all servers
        args.d_verbosity = verbosity;
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        const btes5_Credentials credentials("gooduser", "goodpass");
        args.d_expectedCredentials = credentials;

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        bteso_Endpoint destination;
        args.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &args);

        bteso_Endpoint proxy;  // address of just-started proxy server

        const bdet_TimeInterval proxyTimeout(2.0);
        const bdet_TimeInterval totalTimeout(5.0);

        static const struct {
            int    d_line;                // source line number
            double d_proxy1DelaySeconds;  // delay before every response
            double d_proxy2DelaySeconds;  // delay before every response
            double d_lagSeconds;          // delay before starting attempt
        } DATA[] = {
        //LINE DELAY1  DELAY2    LAG
        //--   ------  ------  -----
        { L_,   0.000,  0.000, 0.000 },  // baseline: no delay

        { L_,   0.000,  0.000, 0.000 },  // simultaneous start with delays
        { L_,   0.010,  0.000, 0.000 },
        { L_,   0.010,  0.010, 0.000 },
        { L_,   0.040,  0.010, 0.000 },
        { L_,   0.040,  0.040, 0.000 },
        { L_,   0.010,  0.040, 0.000 },
        { L_,   0.000,  0.040, 0.000 },

        { L_,   0.000,  0.000, 0.010 },  // lag 10 mS start with delays
        { L_,   0.010,  0.000, 0.010 },
        { L_,   0.010,  0.010, 0.010 },
        { L_,   0.040,  0.010, 0.010 },
        { L_,   0.040,  0.040, 0.010 },
        { L_,   0.010,  0.040, 0.010 },
        { L_,   0.000,  0.040, 0.010 },

        { L_,   0.000,  0.000, 0.040 },  // lag 40 mS start with delays
        { L_,   0.010,  0.000, 0.040 },
        { L_,   0.010,  0.010, 0.040 },
        { L_,   0.040,  0.010, 0.040 },
        { L_,   0.040,  0.040, 0.040 },
        { L_,   0.010,  0.040, 0.040 },
        { L_,   0.000,  0.040, 0.040 },

        };
        const int NUM_DATA = sizeof DATA / sizeof *DATA;

        if (verbose) cout << "Concurrent attempts via 2 connectors" << endl;

        for (int t1 = 0; t1 < NUM_DATA; t1++) {
            const int               LINE  (DATA[t1].d_line);
            const bdet_TimeInterval DELAY1(DATA[t1].d_proxy1DelaySeconds);
            const bdet_TimeInterval DELAY2(DATA[t1].d_proxy2DelaySeconds);
            const bdet_TimeInterval LAG   (DATA[t1].d_lagSeconds);

            if (veryVeryVerbose) { T_ P_(LINE) P_(DELAY1) P_(DELAY2) P(LAG) }

            using namespace bdef_PlaceHolders;

            args.d_delay = DELAY1;
            args.d_label = "proxy1";
            btes5_TestServer proxy1(&proxy, &args);
            btes5_NetworkDescription proxies1;
            proxies1.addProxy(0, proxy, credentials);
            btes5_NetworkConnector connector1(proxies1,
                                              &factory,
                                              &eventManager);

            bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus>
                queue1(1);
            btes5_NetworkConnector::ConnectionStatus status1;
            btes5_NetworkConnector::ConnectionStateCallback
                cb1 = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4,
                                         &queue1);
            btes5_NetworkConnector::AttemptHandle attempt1
              = connector1.makeAttemptHandle(cb1,
                                             proxyTimeout,
                                             totalTimeout,
                                             destination);
            connector1.startAttempt(attempt1);

            args.d_delay = DELAY2;
            args.d_label = "proxy2";
            btes5_TestServer proxy2(&proxy, &args);
            btes5_NetworkDescription proxies2;
            proxies2.addProxy(0, proxy, credentials);
            btes5_NetworkConnector connector2(proxies2,
                                              &factory,
                                              &eventManager);
            bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus>
                queue2(1);
            btes5_NetworkConnector::ConnectionStatus status2;
            btes5_NetworkConnector::ConnectionStateCallback
                cb2 = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4,
                                          &queue2);
            btes5_NetworkConnector::AttemptHandle attempt2
              = connector2.makeAttemptHandle(cb2,
                                             proxyTimeout,
                                             totalTimeout,
                                             destination);
            bcemt_ThreadUtil::sleep(LAG);
            connector2.startAttempt(attempt2);

            // wait for connection results and check for success

            queue1.popFront(&status1);
            LOOP2_ASSERT(LINE, status1,
                         btes5_NetworkConnector::e_SUCCESS == status1);
            queue2.popFront(&status2);
            LOOP2_ASSERT(LINE, status2,
                         btes5_NetworkConnector::e_SUCCESS == status2);
        }

      } break;

      case 5: {
        // --------------------------------------------------------------------
        // UNRESOLVED PROXY
        //
        // Concerns:
        //: 1 Proper timeout processing when a proxy is not resolvable.
        //
        // Plan:
        //: 1 Connect using a bogus proxy and verify error code.
        //
        // Testing:
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "TIMEOUT TEST" << endl
                          << "============" << endl;

        bteso_Endpoint destination;
        btes5_TestServerArgs destinationArgs;
        destinationArgs.d_verbosity = verbosity;
        destinationArgs.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &destinationArgs);

        bteso_Endpoint proxy("unresolvable-proxy.bogus-domain.com", 8100);
        btes5_NetworkDescription proxies;
        proxies.addProxy(0, proxy);

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> queue(1);
        btes5_NetworkConnector::ConnectionStatus status;

        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::ConnectionStateCallback
            cb = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4, &queue);
        {
            if (verbose) cout << "\nUnresolvable proxy" << endl;
            btes5_NetworkConnector connector(proxies, &factory, &eventManager);
            btes5_NetworkConnector::AttemptHandle attempt
                = connector.makeAttemptHandle(cb,
                                              bdet_TimeInterval(1), // proxy
                                              bdet_TimeInterval(5), // total
                                              destination);
            connector.startAttempt(attempt);
            queue.popFront(&status);
            LOOP_ASSERT(status, btes5_NetworkConnector::e_ERROR == status);
        }
      } break;
      case 4: {
        // --------------------------------------------------------------------
        // CASCADED PROXIES
        //  Test of connectivity using 2 levels of proxies.
        //
        // Concerns:
        //: 1 Connections through 2 levels, with a failure in each level.
        //: 2 proper authentication of username and password.
        //
        // Plan:
        //: 1 Using a 'btes5_TestServer' construct a destination server.
        //: 2 Make a 2-level proxy network with one "bad" proxy in each level:
        //:   1 One unresponsive proxy
        //:   2 One proxy with non-matching credentials
        //: 3 Verify successful connection to destination.
        //
        // Testing:
        //   CONCERN: Failover to second proxy after the first proxy failure.
        //   CONCERN: Two proxy levels, one proxy in each level failing.
        //   CONCERN: Connect with construction-time username/password.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "CASCADED PROXIES" << endl
                          << "================" << endl;

        btes5_TestServerArgs args;  // reused for all servers
        args.d_verbosity = verbosity;
        const btes5_Credentials credentials("gooduser", "goodpass");

        bteso_Endpoint destination;
        args.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &args);

        bteso_Endpoint proxy;  // address of just-started proxy server
        btes5_NetworkDescription proxies;

        args.d_label = "unresponsive-0.0";
        args.d_mode = btes5_TestServerArgs::e_IGNORE;
        btes5_TestServer proxy00(&proxy, &args);
        proxies.addProxy(0, proxy, credentials);

        args.d_label = "proxy-0.1";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        btes5_TestServer proxy01(&proxy, &args);
        proxies.addProxy(0, proxy, credentials);

        args.d_label = "bad-credentials-1.0";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        args.d_expectedCredentials.set("baduser", "badpass");
        btes5_TestServer proxy10(&proxy, &args);
        proxies.addProxy(1, proxy, credentials);

        args.d_label = "proxy-1.1";
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        args.d_expectedCredentials.set("gooduser", "goodpass");
        btes5_TestServer proxy11(&proxy, &args);
        proxies.addProxy(1, proxy, credentials);


        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> queue(1);
        btes5_NetworkConnector::ConnectionStatus status;

        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::ConnectionStateCallback
            cb = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4, &queue);
        {
            if (veryVerbose) { P_(destination) P(proxies) }

            btes5_NetworkConnector connector(proxies, &factory, &eventManager);
            btes5_NetworkConnector::AttemptHandle attempt
                = connector.makeAttemptHandle(cb,
                                              bdet_TimeInterval(0.5), // proxy
                                              bdet_TimeInterval(5.0), // total
                                              destination);
            connector.startAttempt(attempt);
            queue.popFront(&status);
            LOOP_ASSERT(status, btes5_NetworkConnector::e_SUCCESS == status);
        }
      } break;
      case 3: {
        // --------------------------------------------------------------------
        // TIMEOUT TEST
        //
        // Concerns:
        //: 1 Proper timeout processing when a proxy is unresponsive.
        //
        // Plan:
        //: 1 Connect using an unresponsive proxy and verify error code:
        //:   1 When the proxy timeout is smaller than total timeout.
        //:   2 When the proxy timeout is greater than total timeout.
        //
        // Testing:
        //   CONCERN: Timeout when the proxy doesn't respond.
        //   CONCERN: Total timeout before proxy timeout.
        //   CONCERN: Failover to second proxy after the first proxy timeout.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "TIMEOUT TEST" << endl
                          << "============" << endl;

        bcema_TestAllocator da("defaultAllocator", veryVeryVerbose);
        bslma::DefaultAllocatorGuard guard(&da);

        bteso_Endpoint destination;
        btes5_TestServerArgs destinationArgs;
        destinationArgs.d_verbosity = verbosity;
        destinationArgs.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &destinationArgs);

        bteso_Endpoint proxy;
        btes5_TestServerArgs proxyArgs;
        proxyArgs.d_verbosity = verbosity;
        proxyArgs.d_label = "unresponsive proxy";
        proxyArgs.d_mode = btes5_TestServerArgs::e_IGNORE;
        btes5_TestServer unresponsiveServer(&proxy, &proxyArgs);

        btes5_NetworkDescription proxies;
        proxies.addProxy(0, proxy);

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> queue(1);
        btes5_NetworkConnector::ConnectionStatus status;

        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::ConnectionStateCallback
            cb = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4, &queue);
        {
            if (verbose) cout << "\nSingle Proxy Timeout" << endl;
            btes5_NetworkConnector connector(proxies, &factory, &eventManager);
            btes5_NetworkConnector::AttemptHandle attempt
                = connector.makeAttemptHandle(cb,
                                              bdet_TimeInterval(2), // proxy
                                              bdet_TimeInterval(5), // total
                                              destination);
            connector.startAttempt(attempt);
            queue.popFront(&status);
            LOOP_ASSERT(status, btes5_NetworkConnector::e_ERROR == status);
        }
        {
            if (verbose) cout << "\nTotal Attempt Timeout" << endl;
            btes5_NetworkConnector connector(proxies, &factory, &eventManager);
            btes5_NetworkConnector::AttemptHandle attempt
                = connector.makeAttemptHandle(cb,
                                              bdet_TimeInterval(5), // proxy
                                              bdet_TimeInterval(2), // total
                                              destination);
            connector.startAttempt(attempt);
            queue.popFront(&status);
            LOOP_ASSERT(status, btes5_NetworkConnector::e_TIMEOUT == status);
        }
        {
            if (verbose) cout << "\nFailover after proxy timeout" << endl;

            // add another, good proxy server to the network description

            proxyArgs.d_label = "good proxy";
            proxyArgs.d_mode = btes5_TestServerArgs::e_CONNECT;
            btes5_TestServer goodServer(&proxy, &proxyArgs);
            proxies.addProxy(0, proxy);

            btes5_NetworkConnector connector(proxies, &factory, &eventManager);
            btes5_NetworkConnector::AttemptHandle attempt
                = connector.makeAttemptHandle(cb,
                                              bdet_TimeInterval(2), // proxy
                                              bdet_TimeInterval(5), // total
                                              destination);
            connector.startAttempt(attempt);
            queue.popFront(&status);
            LOOP_ASSERT(status, btes5_NetworkConnector::e_SUCCESS == status);
        }
      } break;
      case 2: {
        // --------------------------------------------------------------------
        // SOCKS5 SOCKET IMPORT INTO SESSION POOL
        //
        // Concerns:
        //: 1 A socket connection established by 'btes5_NetworkConnector' can
        //:   be imported into, and managed by btemt_SessionPool
        //: 2 Only the specified allocator is used.
        //
        // Plan:
        //: 1 Create a test destination server
        //: 2 Create a test SOCKS5 proxy.
        //: 3 Construct a test allocator to be used by the object under test.
        //: 4 Establish a connection to the destination server via the proxy.
        //: 5 Import the connection into a SessionPool.
        //: 6 Verify that the default allocator has not been used.
        //
        // Testing:
        //   btes5_NetworkConnector(net, fact, eventMgr, *a = 0);
        //   ~btes5_NetworkConnector();
        //   AttemptHandle makeAttemptHandle(cB, proxyTO, totalTO, server);
        //   void startAttempt(AttemptHandle& connectionAttempt);
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "SOCKS5 SOCKET IMPORT INTO SESSION POOL" << endl
                          << "======================================" << endl;

        btes5_TestServerArgs args;
        args.d_verbosity = verbosity;

        bteso_Endpoint destination;
        args.d_mode = btes5_TestServerArgs::e_IGNORE;
        args.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &args);
        if (verbose) {
            cout << "destination server started on " << destination << endl;
        }

        args.d_expectedDestination = destination;
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        args.d_label = "proxy";
        bteso_Endpoint proxy;
        btes5_TestServer proxyServer(&proxy, &args);
        if (verbose) {
            cout << "proxy started on " << proxy << endl;
        }
        btes5_NetworkDescription socks5Servers;
        socks5Servers.addProxy(0, proxy);

        bteso_InetStreamSocketFactory<bteso_IPv4Address> socketFactory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        btemt_ChannelPoolConfiguration config;
        using namespace bdef_PlaceHolders;
        btemt_SessionPool sessionPool(config, &poolStateCb);
        ASSERT(0 == sessionPool.start());
        if (verbose) {
            cout << "starting btemt_SessionPool" << endl;
        }

        if (verbose) {
            cout << "attempting to connect to " << destination
                 << " via " << proxy
                 << endl;
        }
        const bdet_TimeInterval proxyTimeout(10.0);
        bcemt_Mutex     stateLock;
        bcemt_Condition stateChanged;
        volatile int    state = 0; // value > 0 indicates success, < 0 is error
        bcemt_LockGuard<bcemt_Mutex> lock(&stateLock);

        btes5_NetworkConnector::ConnectionStateCallback
            cb = bdef_BindUtil::bind(socks5Cb,
                                      _1, _2, _3, _4,
                                      &sessionPool,
                                      &stateLock,
                                      &stateChanged,
                                      &state);

        btes5_NetworkConnector connector(socks5Servers,
                                         &socketFactory,
                                         &eventManager);

        btes5_NetworkConnector::AttemptHandle attempt
            = connector.makeAttemptHandle(cb,
                                          proxyTimeout, bdet_TimeInterval(),
                                          destination);
        connector.startAttempt(attempt);
        while (!state) {
            stateChanged.wait(&stateLock);
        }
        LOOP_ASSERT(state, state > 0);

      } break;
      case 1: {
        // --------------------------------------------------------------------
        // BREATHING TEST
        //   This case exercises (but does not fully test) basic functionality.
        //
        // Concerns:
        //: 1 The class is sufficiently functional to enable comprehensive
        //:   testing in subsequent test cases.
        //
        // Plan:
        //: 1 Create a test server instance 'destination'.
        //: 2 Create a test server 'proxy'.
        //: 3 Connect to 'destination' through 'proxy' using SOCKS5.
        //: 4 Verify that the callback is invoked with successful result.
        //
        // Testing:
        //   BREATHING TEST
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "BREATHING TEST" << endl
                          << "==============" << endl;

        // install a 'TestAllocator' as defalt to check for memory leaks

        bcema_TestAllocator ta("test1", veryVeryVerbose);
        bslma::DefaultAllocatorGuard guard(&ta);

        bteso_Endpoint destination;
        btes5_TestServerArgs destinationArgs;
        destinationArgs.d_verbosity = verbosity;
        destinationArgs.d_label = "destination";
        btes5_TestServer destinationServer(&destination, &destinationArgs);
        if (verbose) {
            cout << "destination server started on " << destination << endl;
        }

        bteso_Endpoint proxy;
        btes5_TestServerArgs proxyArgs;
        proxyArgs.d_verbosity = verbosity;
        proxyArgs.d_label = "proxy";
        proxyArgs.d_mode = btes5_TestServerArgs::e_CONNECT;
        btes5_TestServer proxyServer(&proxy, &proxyArgs);
        if (verbose) {
            cout << "proxy server started on " << proxy << endl;
        }

        btes5_NetworkDescription proxies;
        proxies.addProxy(0, proxy);

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        btes5_NetworkConnector connector(proxies, &factory, &eventManager);
        const bdet_TimeInterval proxyTimeout(2);
        const bdet_TimeInterval totalTimeout(10);
        bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> queue(1);
        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::AttemptHandle attempt
            = connector.makeAttemptHandle(bdef_BindUtil::bind(breathingTestCb,
                                                              _1, _2, _3, _4,
                                                              &queue),
                                          proxyTimeout,
                                          totalTimeout,
                                          destination);
        connector.startAttempt(attempt);

        // wait for connection result and check for success

        btes5_NetworkConnector::ConnectionStatus status;
        queue.popFront(&status);
        ASSERT(btes5_NetworkConnector::e_SUCCESS == status);
      } break;
      case -1: {
        // --------------------------------------------------------------------
        // OFF-THE-SHELF SOCKS5 SERVER
        //  Test 'btes5_NetworkConnector' using off-the-shelf servers.  Since
        //  we cannot rely on these servers being up and reachable, this is a
        //  manually-executed test.
        //
        // Concerns:
        //: 1 The component can connect through off-the-shelf SOCKS5 proxies.
        //
        // Plan:
        //: 1 Using a table of destinations, connect a SOCSK5 server.
        //
        // Testing:
        //   CONCERN: Negotiate using an off-the-shelf SOCKS5 proxy.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "OFF-THE-SHELF" << endl
                          << "=============" << endl;

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();
        const bdet_TimeInterval proxyTimeout(2);
        const bdet_TimeInterval totalTimeout(3);
        const bteso_Endpoint destination("api1.bloomberg.net", 8394);

        bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus> queue(1);
        using namespace bdef_PlaceHolders;
        btes5_NetworkConnector::ConnectionStateCallback
            cb = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4, &queue);

        const bteso_Endpoint proxy("10.16.21.6", 1080);
        btes5_NetworkDescription proxies;
        proxies.addProxy(0, proxy);
        btes5_NetworkConnector connector(proxies, &factory, &eventManager);

        static const struct {
            int         d_line;     // source line number
            const char *d_hostname;
            int         d_port;
            bool        d_success;  // will connection succeed?
        } DATA[] = {
            //LINE HOSTNAME              PORT  SUCCESS
            //---- --------------------  ----  -------
            { L_,  "208.134.161.62",     8194, true },
            { L_,  "api1.bloomberg.net", 8194, true },
            { L_,  "api1.bloomberg.net", 8394, false },
        };
        const int NUM_DATA = sizeof DATA / sizeof *DATA;
        for (int i = 0; i < NUM_DATA; i++) {
            const int LINE = DATA[i].d_line;
            const bteso_Endpoint DESTINATION(DATA[i].d_hostname,
                                             DATA[i].d_port);
            const bool SUCCESS = DATA[i].d_success;

            if (veryVerbose) { T_ P_(proxy) P(DESTINATION) }

            btes5_NetworkConnector::AttemptHandle attempt
                = connector.makeAttemptHandle(cb,
                                              proxyTimeout,
                                              totalTimeout,
                                              DESTINATION);
            connector.startAttempt(attempt);

            // wait for connection result and check for success

            btes5_NetworkConnector::ConnectionStatus status;
            queue.popFront(&status);
            LOOP4_ASSERT(proxy, DESTINATION, status, SUCCESS,
                     SUCCESS == (status == btes5_NetworkConnector::e_SUCCESS));
        }
      } break;
      case -2: {
        // --------------------------------------------------------------------
        // OVERLAPPED CONNECTIONS ON ONE CONNECTOR
        //  Test concurrency through overlapping connection attempts.
        //
        // Concerns:
        //: 1 Two connections can be established concurrently, using the same
        //:   connector.
        //
        // Plan:
        //: 1 Using a table-driven method:
        //:   1 Create a connector with a two-level network with given delays.
        //:   2 Start one connection attempt with this connector.
        //:   3 After a lag given in the table start the second attempt.
        //:   4 Check for successful connection from both attempts.
        //
        // Testing:
        //   CONCERN: Two overlapping connection attempts on one connector.
        // --------------------------------------------------------------------

        if (verbose) cout << endl
                          << "OVERLAPPED CONNECTIONS ON ONE CONNECTOR" << endl
                          << "=======================================" << endl;

        bcema_TestAllocator defaultAllocator("default", veryVeryVeryVerbose);
        bslma::Default::setDefaultAllocatorRaw(&defaultAllocator);

        btes5_TestServerArgs args;  // reused for all servers
        args.d_verbosity = verbosity;
        args.d_mode = btes5_TestServerArgs::e_CONNECT;
        const btes5_Credentials credentials("gooduser", "goodpass");
        args.d_expectedCredentials = credentials;

        bteso_InetStreamSocketFactory<bteso_IPv4Address> factory;
        btemt_TcpTimerEventManager eventManager;
        eventManager.enable();

        bteso_Endpoint destination1;
        args.d_label = "destination1";
        btes5_TestServer destinationServer1(&destination1, &args);

        bteso_Endpoint destination2;
        args.d_label = "destination2";
        btes5_TestServer destinationServer2(&destination2, &args);

        bteso_Endpoint proxy;  // address of just-started proxy server

        const bdet_TimeInterval proxyTimeout(2.0);
        const bdet_TimeInterval totalTimeout(5.0);

        static const struct {
            int    d_line;                // source line number
            double d_proxy1DelaySeconds;  // delay before every response
            double d_proxy2DelaySeconds;  // delay before every response
            double d_lagSeconds;          // delay before starting 2nd attempt
        } DATA[] = {
        //LINE DELAY1  DELAY2    LAG
        //--   ------  ------  -----
        { L_,   0.000,  0.000, 0.000 },  // baseline: no delay

        { L_,   0.000,  0.000, 0.000 },  // simultaneous start with delays
        { L_,   0.010,  0.000, 0.000 },
        { L_,   0.010,  0.010, 0.000 },
        { L_,   0.040,  0.010, 0.000 },
        { L_,   0.040,  0.040, 0.000 },
        { L_,   0.010,  0.040, 0.000 },
        { L_,   0.000,  0.040, 0.000 },

        { L_,   0.000,  0.000, 0.010 },  // lag 10 mS start with delays
        { L_,   0.010,  0.000, 0.010 },
        { L_,   0.010,  0.010, 0.010 },
        { L_,   0.040,  0.010, 0.010 },
        { L_,   0.040,  0.040, 0.010 },
        { L_,   0.010,  0.040, 0.010 },
        { L_,   0.000,  0.040, 0.010 },

        { L_,   0.000,  0.000, 0.040 },  // lag 40 mS start with delays
        { L_,   0.010,  0.000, 0.040 },
        { L_,   0.010,  0.010, 0.040 },
        { L_,   0.040,  0.010, 0.040 },
        { L_,   0.040,  0.040, 0.040 },
        { L_,   0.010,  0.040, 0.040 },
        { L_,   0.000,  0.040, 0.040 },

        };
        const int NUM_DATA = sizeof DATA / sizeof *DATA;

        for (int t1 = 0; t1 < NUM_DATA; t1++) {
            const int               LINE  (DATA[t1].d_line);
            const bdet_TimeInterval DELAY1(DATA[t1].d_proxy1DelaySeconds);
            const bdet_TimeInterval DELAY2(DATA[t1].d_proxy2DelaySeconds);
            const bdet_TimeInterval LAG   (DATA[t1].d_lagSeconds);

            if (veryVeryVerbose) { T_ P_(LINE) P_(DELAY1) P_(DELAY2) P(LAG) }

            using namespace bdef_PlaceHolders;

            // construct a connector with a 2-level proxy network

            btes5_NetworkDescription proxies;

            args.d_delay = DELAY1;
            args.d_label = "proxy1";
            btes5_TestServer proxy1(&proxy, &args);
            proxies.addProxy(0, proxy, credentials);

            args.d_delay = DELAY2;
            args.d_label = "proxy2";
            btes5_TestServer proxy2(&proxy, &args);
            proxies.addProxy(1, proxy, credentials);

            btes5_NetworkConnector connector(proxies,
                                              &factory,
                                              &eventManager);

            // create a queue to convey status and start the 1st connection

            bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus>
                queue1(1);
            btes5_NetworkConnector::ConnectionStatus status1;
            btes5_NetworkConnector::ConnectionStateCallback
                cb1 = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4,
                                         &queue1);
            btes5_NetworkConnector::AttemptHandle attempt1
              = connector.makeAttemptHandle(cb1,
                                             proxyTimeout,
                                             totalTimeout,
                                             destination1);
            connector.startAttempt(attempt1);

            // create a queue and, after a lag, start the 2nd connection

            bcec_FixedQueue<btes5_NetworkConnector::ConnectionStatus>
                queue2(1);
            btes5_NetworkConnector::ConnectionStatus status2;
            btes5_NetworkConnector::ConnectionStateCallback
                cb2 = bdef_BindUtil::bind(breathingTestCb, _1, _2, _3, _4,
                                          &queue2);
            btes5_NetworkConnector::AttemptHandle attempt2
              = connector.makeAttemptHandle(cb2,
                                             proxyTimeout,
                                             totalTimeout,
                                             destination2);
            bcemt_ThreadUtil::sleep(LAG);
            connector.startAttempt(attempt2);

            // wait for connection results and check for success

            queue1.popFront(&status1);
            LOOP2_ASSERT(LINE, status1,
                         btes5_NetworkConnector::e_SUCCESS == status1);
            queue2.popFront(&status2);
            LOOP2_ASSERT(LINE, status2,
                         btes5_NetworkConnector::e_SUCCESS == status2);
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

// ----------------------------------------------------------------------------
// NOTICE:
//      Copyright (C) Bloomberg L.P., 2011
//      All Rights Reserved.
//      Property of Bloomberg L.P. (BLP)
//      This software is made available solely pursuant to the
//      terms of a BLP license agreement which governs its use.
// ----------------------------- END-OF-FILE ----------------------------------
