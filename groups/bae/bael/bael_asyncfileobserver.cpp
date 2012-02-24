// bael_asyncfileobserver.cpp                                         -*-C++-*-
#include <bael_asyncfileobserver.h>

#include <bdes_ident.h>
BDES_IDENT_RCSID(bael_asyncfileobserver_cpp,"$Id$ $CSID$")

#ifdef FOR_TESTING_ONLY
#include <bael_defaultobserver.h>             // for testing only
#include <bael_log.h>                         // for testing only
#include <bael_loggermanager.h>               // for testing only
#include <bael_loggermanagerconfiguration.h>  // for testing only
#include <bael_multiplexobserver.h>           // for testing only
#endif

#include <bcemt_lockguard.h>

#include <bdef_function.h>
#include <bdef_bind.h>
#include <bdef_memfn.h>

#include <bslma_default.h>

namespace BloombergLP {

                       // ----------------------------
                       // class bael_AsyncFileObserver
                       // ----------------------------

// PRIVATE METHODS
void bael_AsyncFileObserver::publishThreadEntryPoint()
{
    while (1)
    {
        AsyncRecord asyncRecord = d_recordQueue.popFront();
        if (bael_Transmission::BAEL_END
                == asyncRecord.d_context.transmissionCause()
            || d_clearing) {
            break;
        }
        d_fileObserver.publish(*asyncRecord.d_record, asyncRecord.d_context);
    }
}

int bael_AsyncFileObserver::startThread()
{
    if (bcemt_ThreadUtil::invalidHandle() == d_threadHandle)
    {
        bcemt_ThreadAttributes attr;
        return bcemt_ThreadUtil::create(&d_threadHandle,
                                        attr,
                                        d_publishThreadEntryPoint);   // RETURN
    }
    return 0;
}

int bael_AsyncFileObserver::stopThread()
{
    if (bcemt_ThreadUtil::invalidHandle() != d_threadHandle)
    {
        // Push an empty record with BAEL_END set in context

        bcema_SharedPtr<const bael_Record> record(
                new (*d_allocator_p) bael_Record(d_allocator_p),
                d_allocator_p);
        bael_Context context(bael_Transmission::BAEL_END, 0, 1);
        publish(record, context);
        int ret = bcemt_ThreadUtil::join(d_threadHandle);
        d_threadHandle = bcemt_ThreadUtil::invalidHandle();
        return ret;                                                   // RETURN
    }
    return 0;
}


// CREATORS
bael_AsyncFileObserver::bael_AsyncFileObserver(
                bael_Severity::Level  stdoutThreshold,
                bslma_Allocator      *basicAllocator)
: d_fileObserver(stdoutThreshold, basicAllocator)
, d_threadHandle(bcemt_ThreadUtil::invalidHandle())
, d_clearing(false)
, d_recordQueue(8192, basicAllocator)
, d_allocator_p(bslma_Default::globalAllocator(basicAllocator))
{
    d_publishThreadEntryPoint
        = bdef_Function<void (*)()>(
              bdef_MemFnUtil::memFn(
                      &bael_AsyncFileObserver::publishThreadEntryPoint,
                      this),
              d_allocator_p);
}

bael_AsyncFileObserver::bael_AsyncFileObserver(
                bael_Severity::Level  stdoutThreshold,
                bool                  publishInLocalTime,
                int                   fixedQueueSize,
                bslma_Allocator      *basicAllocator)
: d_fileObserver(stdoutThreshold, publishInLocalTime, basicAllocator)
, d_threadHandle(bcemt_ThreadUtil::invalidHandle())
, d_clearing(false)
, d_recordQueue(fixedQueueSize, basicAllocator)
, d_allocator_p(bslma_Default::globalAllocator(basicAllocator))

{
    d_publishThreadEntryPoint
        = bdef_Function<void (*)()>(
              bdef_MemFnUtil::memFn(
                      &bael_AsyncFileObserver::publishThreadEntryPoint,
                      this),
              d_allocator_p);
}

bael_AsyncFileObserver::~bael_AsyncFileObserver()
{
    stopPublicationThread();
}

// MANIPULATORS
void bael_AsyncFileObserver::releaseRecords()
{
    bcemt_LockGuard<bcemt_Mutex> guard(&d_mutex);
    d_clearing = true;
    stopThread();
    d_recordQueue.removeAll();
    d_clearing = false;
    startThread();
}

void bael_AsyncFileObserver::publish(
                const bcema_SharedPtr<const bael_Record>& record,
                const bael_Context&                       context)
{
    AsyncRecord asyncRecord;
    asyncRecord.d_record  = record;
    asyncRecord.d_context = context;
    d_recordQueue.pushBack(asyncRecord);
}

int bael_AsyncFileObserver::startPublicationThread()
{
    bcemt_LockGuard<bcemt_Mutex> guard(&d_mutex);
    return startThread();
}

int bael_AsyncFileObserver::stopPublicationThread()
{
    bcemt_LockGuard<bcemt_Mutex> guard(&d_mutex);
    return stopThread();
}

int bael_AsyncFileObserver::shutdownPublicationThread()
{
    bcemt_LockGuard<bcemt_Mutex> guard(&d_mutex);
    d_clearing = true;
    return stopThread();
    d_clearing = false;
}


}  // close namespace BloombergLP

// ---------------------------------------------------------------------------
// NOTICE:
//      Copyright (C) Bloomberg L.P., 2012
//      All Rights Reserved.
//      Property of Bloomberg L.P. (BLP)
//      This software is made available solely pursuant to the
//      terms of a BLP license agreement which governs its use.
// ----------------------------- END-OF-FILE ---------------------------------
