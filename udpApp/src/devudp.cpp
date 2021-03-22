/*************************************************************************\
* Copyright (c) 2021 Brookhaven Science Assoc. as operator of
      Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <epicsString.h>
#include <epicsAtomic.h>

#include <lsoRecord.h>
#include <lsiRecord.h>
#include <boRecord.h>
#include <biRecord.h>
#include <aiRecord.h>
#include <int64inRecord.h>

#include "psc/devcommon.h"
#include "udpdrv.h"

#include <epicsExport.h>

namespace {

long devudp_init_record_period(aiRecord *prec)
{
    epicsTimeStamp *pvt = (epicsTimeStamp*)malloc(sizeof(*pvt));
    if(pvt)
        epicsTimeGetCurrent(pvt);
    prec->dpvt = pvt;
    return 0;
}

long devudp_interval(aiRecord *prec)
{
    epicsTimeStamp *pvt = (epicsTimeStamp*)prec->dpvt;
    epicsTimeStamp now;
    if(pvt && epicsTimeGetCurrent(&now)==0) {
        prec->val = epicsTimeDiffInSeconds(&now, pvt);
        *pvt = now;
        return 2;

    } else {
        (void)recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        prec->val = 0.0;
        return 2;
    }
}

template<typename REC>
long devudp_init_record_out(REC *prec)
{
    try {
        DBLINK* plink = &prec->out;
        UDPFast* dev = PSCBase::getPSC<UDPFast>(plink->value.instio.string);
        if(!dev) {
            timefprintf(stderr, "%s: can't find UDPFast '%s'\n", prec->name, plink->value.instio.string);
        }
        prec->dpvt = (void*)dev;
        return 0;
    }CATCH(devudp_init_record, prec);
}

template<typename REC>
long devudp_init_record_in(REC *prec)
{
    try {
        DBLINK* plink = &prec->inp;
        UDPFast* dev = PSCBase::getPSC<UDPFast>(plink->value.instio.string);
        if(!dev) {
            timefprintf(stderr, "%s: can't find UDPFast '%s'\n", prec->name, plink->value.instio.string);
        }
        prec->dpvt = (void*)dev;
        return 0;
    }CATCH(devudp_init_record, prec);
}

#define TRY if(!prec->dpvt) return -1; UDPFast *dev = (UDPFast*)prec->dpvt; (void)dev; try

template<std::string UDPFast::*STR>
long devudp_set_string(lsoRecord* prec)
{
    TRY {
        std::string& str = dev->*STR;

        prec->val[prec->sizv - 1u] = '\0'; // paranoia
        std::string newv(prec->val);

        Guard G(dev->lock);
        str = newv;

        return 0;
    }CATCH(devudp_set_string, prec);
}

long devudp_reopen(boRecord* prec)
{
    TRY {
        {
            Guard G(dev->lock);
            dev->reopen = true;
        }
        dev->pendingReady.signal();
        return 0;
    }CATCH(devudp_reopen, prec);
}

long devudp_set_record(boRecord* prec)
{
    TRY {
        {
            Guard G(dev->lock);
            dev->record = prec->val;
            dev->reopen = dev->record;
        }
        if(prec->val)
            dev->pendingReady.signal();
        return 0;
    }CATCH(devudp_set_record, prec);
}

long devudp_get_record(biRecord* prec)
{
    TRY {
        Guard G(dev->lock);
        prec->rval = dev->record;
        return 0;
    }CATCH(devudp_get_record, prec);
}

template<const std::string UDPFast::*STR>
long devudp_get_string(lsiRecord* prec)
{
    TRY {
        const std::string& str = dev->*STR;

        Guard G(dev->lock);

        size_t s = str.size();
        if(s>=prec->sizv) { // truncating
            s = prec->sizv-1u;
            (void)recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        }
        memcpy(prec->val, str.c_str(), s);
        prec->val[s] = '\0';
        prec->len = s+1u;
        return 0;
    }CATCH(devudp_get_string, prec);
}

long devudp_get_vpool(aiRecord* prec)
{
    TRY {
        double val;
        {
            Guard G(dev->lock);
            val = dev->vpool.size()/double(dev->vpoolTotal);
        }
        prec->val = analogRaw2EGU<double>(prec, val);
        return 2;
    }CATCH(devudp_get_vpool, prec);
}

long devudp_get_pending(aiRecord* prec)
{
    TRY {
        double val;
        {
            Guard G(dev->lock);
            val = dev->pending.size()/double(dev->vpoolTotal);
        }
        prec->val = analogRaw2EGU<double>(prec, val);
        return 2;
    }CATCH(devudp_get_pending, prec);
}

long devudp_get_inprog(aiRecord* prec)
{
    TRY {
        double val;
        {
            Guard G(dev->lock);
            epicsInt64 total = dev->vpoolTotal,
                       inuse = dev->vpool.size() + dev->pending.size();
            val = (total-inuse)/double(total);
        }
        prec->val = analogRaw2EGU<double>(prec, val);
        return 2;
    }CATCH(devudp_get_inprog, prec);
}

template<size_t UDPFast::*CNT>
long devudp_get_counter(int64inRecord *prec)
{
    TRY {
        prec->val = epicsAtomicGetSizeT(&(dev->*CNT));
        return 0;
    }CATCH(devudp_get_ ## NAME, prec);
}

long devudp_get_lastsize(int64inRecord* prec)
{
    TRY {
        prec->val = epicsAtomicGetSizeT(&dev->lastsize)/(1u<<20u);
        return 0;
    }CATCH(devudp_get_lastsize, prec);
}

MAKEDSET(ai, devPSCUDPIntervalAI, &devudp_init_record_period, 0, &devudp_interval);
MAKEDSET(lso, devPSCUDPFilebaseLSO, &devudp_init_record_out, 0, &devudp_set_string<&UDPFast::filebase>);
MAKEDSET(lso, devPSCUDPFiledirLSO, &devudp_init_record_out, 0, &devudp_set_string<&UDPFast::filedir>);
MAKEDSET(bo, devPSCUDPReopenBO, &devudp_init_record_out, 0, &devudp_reopen);
MAKEDSET(bo, devPSCUDPRecordBO, &devudp_init_record_out, 0, &devudp_set_record);
MAKEDSET(bi, devPSCUDPRecordBI, &devudp_init_record_in, 0, &devudp_get_record);
MAKEDSET(lsi, devPSCUDPFilenameLSI, &devudp_init_record_in, 0, &devudp_get_string<&UDPFast::lastfile>);
MAKEDSET(lsi, devPSCUDPErrorLSI, &devudp_init_record_in, 0, &devudp_get_string<&UDPFast::lasterror>);
MAKEDSET(ai, devPSCUDPvpoolAI, &devudp_init_record_in, 0, &devudp_get_vpool);
MAKEDSET(ai, devPSCUDPpendingAI, &devudp_init_record_in, 0, &devudp_get_pending);
MAKEDSET(ai, devPSCUDPinprogAI, &devudp_init_record_in, 0, &devudp_get_inprog);
MAKEDSET(int64in, devPSCUDPnetrxI64I, &devudp_init_record_in, 0, &devudp_get_counter<&UDPFast::netrx>);
MAKEDSET(int64in, devPSCUDPwroteI64I, &devudp_init_record_in, 0, &devudp_get_counter<&UDPFast::storewrote>);
MAKEDSET(int64in, devPSCUDPndropI64I, &devudp_init_record_in, 0, &devudp_get_counter<&UDPFast::ndrops>);
MAKEDSET(int64in, devPSCUDPlastsizeI64I, &devudp_init_record_in, 0, &devudp_get_lastsize);
MAKEDSET(int64in, devPSCUDPnrxI64I, &devudp_init_record_in, 0, &devudp_get_counter<&UDPFast::rxcnt>);
MAKEDSET(int64in, devPSCUDPntimeoutI64I, &devudp_init_record_in, 0, &devudp_get_counter<&UDPFast::ntimeout>);
MAKEDSET(int64in, devPSCUDPnoomI64I, &devudp_init_record_in, 0, &devudp_get_counter<&UDPFast::noom>);

}

extern "C" {
epicsExportAddress(dset, devPSCUDPIntervalAI);
epicsExportAddress(dset, devPSCUDPFilebaseLSO);
epicsExportAddress(dset, devPSCUDPFiledirLSO);
epicsExportAddress(dset, devPSCUDPReopenBO);
epicsExportAddress(dset, devPSCUDPRecordBO);
epicsExportAddress(dset, devPSCUDPRecordBI);
epicsExportAddress(dset, devPSCUDPFilenameLSI);
epicsExportAddress(dset, devPSCUDPErrorLSI);
epicsExportAddress(dset, devPSCUDPvpoolAI);
epicsExportAddress(dset, devPSCUDPpendingAI);
epicsExportAddress(dset, devPSCUDPinprogAI);
epicsExportAddress(dset, devPSCUDPnetrxI64I);
epicsExportAddress(dset, devPSCUDPwroteI64I);
epicsExportAddress(dset, devPSCUDPndropI64I);
epicsExportAddress(dset, devPSCUDPlastsizeI64I);
epicsExportAddress(dset, devPSCUDPnrxI64I);
epicsExportAddress(dset, devPSCUDPntimeoutI64I);
epicsExportAddress(dset, devPSCUDPnoomI64I);
}