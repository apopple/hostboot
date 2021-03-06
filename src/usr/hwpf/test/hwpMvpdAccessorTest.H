/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/hwpf/test/hwpMvpdAccessorTest.H $                     */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* COPYRIGHT International Business Machines Corp. 2012,2014              */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */

#ifndef __HWPMVPDACCESSORTEST_H
#define __HWPMVPDACCESSORTEST_H

// set to 1 for doing unit tests, set to 0 for production
#define HWPMVPDACCESSORTEST_UT 0

/**
 *  @file hwpMvpdAccessorTest.H
 *
 *  @brief Test cases for Mvpd HWP accessors.
*/

#include <cxxtest/TestSuite.H>

#include    <fapi.H>
#include    <fapiPlatHwpInvoker.H>

#include    <targeting/common/commontargeting.H>
#include    <targeting/common/utilFilter.H>

#include    <getMvpdRing.H>
#include    <setMvpdRing.H>

#include <errl/errlmanager.H>
#include <errl/errlentry.H>
#include <devicefw/driverif.H>
#include <trace/interface.H>

//      pull in CompressedScanData def from proc_slw_build HWP
#include <p8_scan_compression.H>

using namespace     fapi;
using   namespace   TARGETING;

class hwpMvpdAccessorTest: public CxxTest::TestSuite
{
public:

    /**
     * @brief   call fapiGetMvpdField to fetch a mvpd records.
     *
     */
    void    testGetMvpd()
    {
        fapi::ReturnCode    l_fapirc( fapi::FAPI_RC_SUCCESS );
        uint8_t             *l_pdRRecord            =   NULL;
        uint32_t            l_pdRLen                =   0;

        // list of MVPD records to test. Need to be in PNOR or procmvpd.dat
        // when g_usePNOR is false.
        struct _testMvpdRecords {
            fapi::MvpdRecord record;
            fapi::MvpdKeyword keyword;
        }  l_mvpdRecords[] = {
#if HWPMVPDACCESSORTEST_UT
            { MVPD_RECORD_CP00, MVPD_KEYWORD_PDG},
            { MVPD_RECORD_MER0, MVPD_KEYWORD_PDI},
//          { MVPD_RECORD_VER0, MVPD_KEYWORD_PDI}, //VER0 in spec,not supported
#endif
            { MVPD_RECORD_VWML, MVPD_KEYWORD_PDI},
        };

        TS_TRACE( "testGetMvpd entry" );

        TARGETING::TargetHandleList l_cpuTargetList;
        getAllChips(l_cpuTargetList, TYPE_PROC);

        TS_TRACE( "testGetMvpd l_cpuTargetList.size()= 0x%x ",
                                      l_cpuTargetList.size() );


        // loop thru all the cpu's
        for (TargetHandleList::iterator l_cpu_iter = l_cpuTargetList.begin();
                l_cpu_iter != l_cpuTargetList.end();
                ++l_cpu_iter)
        {
            //  make a local copy of the CPU target
            TARGETING::Target* l_cpu_target = *l_cpu_iter;

            TS_TRACE( "target HUID %.8X", TARGETING::get_huid(l_cpu_target));

            // cast OUR type of target to a FAPI type of target.
            fapi::Target l_fapi_cpu_target(
                                          TARGET_TYPE_PROC_CHIP,
                   (const_cast<TARGETING::Target*>(l_cpu_target)) );

            // loop through mvpd records of interest
            const uint32_t numRecords =
                    sizeof(l_mvpdRecords)/sizeof(l_mvpdRecords[0]);
            for (uint8_t i=0;i<numRecords;i++) {

                TS_TRACE( "record = 0x%x keyword = 0x%x",
                   l_mvpdRecords[i].record,
                   l_mvpdRecords[i].keyword);

                TS_TRACE( "call fapiGetMvpdField with NULL pointer" );

                //  call fapiGetMvpdField once with a NULL pointer to get the
                //  buffer size should return no error now.
                l_fapirc = fapiGetMvpdField(l_mvpdRecords[i].record,
                                        l_mvpdRecords[i].keyword,
                                        l_fapi_cpu_target,
                                        NULL,
                                        l_pdRLen );
                if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
                {
                    TS_FAIL( "fapiGetMvpdField:  expected FAPI_RC_SUCCESS" );
                    fapiLogError(l_fapirc);
                    return;
                }

                TS_TRACE( "fapiGetMvpdField: size of record = 0x%x",
                      l_pdRLen );

                //  do a malloc instead of a new just for variety
                l_pdRRecord =   reinterpret_cast<uint8_t *>(malloc(l_pdRLen) );

                //  call fapiGetMvpdField once with a valid pointer
                l_fapirc = fapiGetMvpdField(l_mvpdRecords[i].record,
                                        l_mvpdRecords[i].keyword,
                                        l_fapi_cpu_target,
                                        l_pdRRecord,
                                        l_pdRLen );
                if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
                {
                    TS_FAIL( "fapiGetMvpdField:  expected FAPI_RC_SUCCESS" );
                    fapiLogError(l_fapirc);
                    free( l_pdRRecord );
                    return;
                }

                //  clean up memory
                free( l_pdRRecord );
            }
        }

        TS_TRACE( "testGetMvpd exit" );
    }


    // Structure used to save/restore the VPD
    typedef struct saveRestoreData_t {
        TARGETING::Target* target;
        uint8_t* CP00_pdG;
        size_t CP00_pdG_size;
        uint8_t* CP00_pdR;
        size_t CP00_pdR_size;
    } saveRestoreData_t;

    /**
     * @brief Test get and set of Repair Rings
     */
    void testRepairRings()
    {
        fapi::ReturnCode    l_fapirc        =   fapi::FAPI_RC_SUCCESS;
        uint8_t             *l_pRingBuf     =   NULL;
        uint32_t            l_ringBufsize   =   0;
        uint32_t            l_ringId        =   0;
        uint32_t            l_chipletId     =   0;
        uint32_t            l_bufsize       =   0x200;
        errlHndl_t l_errhdl = NULL;

        // This data needs to be in sync with the procmvpd.dat file
        // the setMvpdFunc tests use the last row. The test
        //  expects it to be a mid x20 byte ring in the #G keyword
        struct _testRRstr {
            fapi::MvpdKeyword keyword;
            uint32_t ringIdval;
            uint32_t chipletIdval;
            uint32_t size;
            uint32_t rc;
        }  l_ringModifiers[] = {
            { MVPD_KEYWORD_PDG, 0xa4, 0xFF, 0x20,  //last #G
                                fapi::RC_REPAIR_RING_NOT_FOUND },
            { MVPD_KEYWORD_PDR, 0xe0, 0x08, 0x20, //first #R
                                FAPI_RC_SUCCESS },
            { MVPD_KEYWORD_PDR, 0xe2, 0x16, 0x20, //big #R
                                FAPI_RC_SUCCESS },
            { MVPD_KEYWORD_PDG, 0xa2, 0x08, 0x20, //mid #G
                                FAPI_RC_SUCCESS },
            { MVPD_KEYWORD_PDR, 0xe1, 0x16, 0x20,//big #R
                                FAPI_RC_SUCCESS },
        };
        const size_t VALID_INDEX = 2;
        const size_t TEST_INDEX = 4;

        TS_TRACE( "testRepairRings entry" );

        std::list<saveRestoreData_t> l_srData;

        TARGETING::TargetHandleList l_cpuTargetList;
        getAllChips(l_cpuTargetList, TYPE_PROC);

        // loop thru all the cpu's
        for (TargetHandleList::iterator l_cpu_iter = l_cpuTargetList.begin();
                l_cpu_iter != l_cpuTargetList.end();
                ++l_cpu_iter)
        {
            //  make a local copy of the CPU target
            TARGETING::Target* l_cpu_target = *l_cpu_iter;

            TS_TRACE( "testRepairRings: "
                "target HUID %.8X",
                TARGETING::get_huid(l_cpu_target));

            //--  Save the entire VPD record to restore later
            saveRestoreData_t tmpsave;
            tmpsave.target = l_cpu_target;

            // do a read with NULL buffer to get the record size
            l_errhdl = deviceRead( l_cpu_target,
                                   NULL,
                                   tmpsave.CP00_pdG_size,
                                   DEVICE_MVPD_ADDRESS( MVPD_RECORD_CP00,
                                                        MVPD_KEYWORD_PDG ) );
            if( l_errhdl )
            {
                TS_FAIL("Error finding size of CP00/#G for %.8X",
                        TARGETING::get_huid(l_cpu_target));
                errlCommit( l_errhdl, VPD_COMP_ID );
                continue;
            }

            // now go get the data
            tmpsave.CP00_pdG = new uint8_t[tmpsave.CP00_pdG_size];
            l_errhdl = deviceRead( l_cpu_target,
                                   tmpsave.CP00_pdG,
                                   tmpsave.CP00_pdG_size,
                                   DEVICE_MVPD_ADDRESS( MVPD_RECORD_CP00,
                                                        MVPD_KEYWORD_PDG ) );
            if( l_errhdl )
            {
                TS_FAIL("Error reading CP00/#G from %.8X",
                        TARGETING::get_huid(l_cpu_target));
                errlCommit( l_errhdl, VPD_COMP_ID );
                delete[] tmpsave.CP00_pdG;
                continue;
            }

            // do a read with NULL buffer to get the record size
            l_errhdl = deviceRead( l_cpu_target,
                                   NULL,
                                   tmpsave.CP00_pdR_size,
                                   DEVICE_MVPD_ADDRESS( MVPD_RECORD_CP00,
                                                        MVPD_KEYWORD_PDR ) );
            if( l_errhdl )
            {
                TS_FAIL("Error finding size of CP00/#R for %.8X",
                        TARGETING::get_huid(l_cpu_target));
                errlCommit( l_errhdl, VPD_COMP_ID );
                continue;
            }

            // now go get the data
            tmpsave.CP00_pdR = new uint8_t[tmpsave.CP00_pdR_size];
            l_errhdl = deviceRead( l_cpu_target,
                                   tmpsave.CP00_pdR,
                                   tmpsave.CP00_pdR_size,
                                   DEVICE_MVPD_ADDRESS( MVPD_RECORD_CP00,
                                                        MVPD_KEYWORD_PDR ) );
            if( l_errhdl )
            {
                TS_FAIL("Error reading CP00/#R from %.8X",
                        TARGETING::get_huid(l_cpu_target));
                errlCommit( l_errhdl, VPD_COMP_ID );
                delete[] tmpsave.CP00_pdR;
                continue;
            }

            // add to the master list
            l_srData.push_back(tmpsave);

            // cast OUR type of target to a FAPI type of target.
            fapi::Target l_fapi_cpu_target(
                              TARGET_TYPE_PROC_CHIP,
                              (const_cast<TARGETING::Target*>(l_cpu_target)) );

            //  allocate some space to put the record(s)
            if( l_pRingBuf != NULL )
            {
                delete[] l_pRingBuf;
            }
            l_pRingBuf      =    new uint8_t[l_bufsize];

            //  ----------------------------------------------------------------
            //  Pass in 0 for the ring modifier, should return with "not found"
            //  error
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: pass in invalid ringId" );
            l_ringBufsize   =   l_bufsize;
            l_ringId        =   0;     // ringId
            l_chipletId     =   0;     // chipletId
            l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );
            TS_TRACE("testRepairRings:ringId=0x%x chipletId=0x%x Bufsize=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            if ( l_fapirc != fapi::RC_REPAIR_RING_NOT_FOUND )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL("testRepairRings: expect not found rc FAIL: 0x%x, 0x%x",
                         fapi::RC_REPAIR_RING_NOT_FOUND,
                         static_cast<uint32_t>(l_fapirc) );
                fapiLogError(l_fapirc);
            }

            //  ----------------------------------------------------------------
            //  Pass in a buffer size that is too small with a valid
            //  ringId/chipletId, should return error with correct length
            //  and invalid size return code..
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: pass buffer too small %d ",
                    VALID_INDEX );
            l_ringBufsize   =   0x0;
            l_ringId        =   l_ringModifiers[VALID_INDEX].ringIdval;
            l_chipletId     =   l_ringModifiers[VALID_INDEX].chipletIdval;
            l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );
            TS_TRACE("testRepairRings:ringId=0x%x chipletId=0x%x: Bufsize=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            if ( l_fapirc != fapi::RC_REPAIR_RING_INVALID_SIZE )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL("testRepairRings: expect invalid size FAIL: exp=0x%x,"
                         " act=0x%x, ring=0x%X",
                         fapi::RC_REPAIR_RING_INVALID_SIZE,
                         static_cast<uint32_t>(l_fapirc),
                         l_ringId);

                fapiLogError(l_fapirc);
            }
            else if ( l_ringBufsize != l_ringModifiers[VALID_INDEX].size )
            {
                TS_FAIL( "testRepairRings: size mismatch FAIL1 on ring 0x%X:"
                         " exp=0x%x, act=0x%x",
                         l_ringId,
                         l_ringModifiers[VALID_INDEX].size,
                         l_ringBufsize  );
            }

            //  ----------------------------------------------------------------
            //  Pass in a NULL pointer with a valid ringId/chipletId, should
            //  return with correct length and successful return code.
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: get size of ring %d ", VALID_INDEX );
            l_ringBufsize   =   0x0;
            l_ringId        =   l_ringModifiers[VALID_INDEX].ringIdval;
            l_chipletId     =   l_ringModifiers[VALID_INDEX].chipletIdval;
            l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          NULL,
                                          l_ringBufsize );
            TS_TRACE("testRepairRings:ringId=0x%x chipletId=0x%x: Bufsize=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            if ( l_fapirc )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings: expect success rc FAIL: 0x%x, 0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc));

                fapiLogError(l_fapirc);
            }
            else if ( l_ringBufsize != l_ringModifiers[VALID_INDEX].size )
            {
                TS_FAIL( "testRepairRings: size mismatch FAIL2 on ring 0x%X:"
                         " exp=0x%x, act=0x%x",
                         l_ringId,
                         l_ringModifiers[VALID_INDEX].size,
                         l_ringBufsize  );
            }

            //  ----------------------------------------------------------------
            //  Fetch rings
            //  ----------------------------------------------------------------
            const uint32_t numRings =
                        sizeof(l_ringModifiers)/sizeof(l_ringModifiers[0]);
            for (size_t i=0;i<numRings;i++)
            {
                TS_TRACE( "testRepairRings: get ring %d", i );
                l_ringBufsize   =   l_bufsize;
                l_ringId    =   l_ringModifiers[i].ringIdval;
                l_chipletId =   l_ringModifiers[i].chipletIdval;
                l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          l_ringModifiers[i].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );
                TS_TRACE("testRepairRings ringId=0x%x chipletId=0x%x size=0x%x",
                      l_ringId, l_chipletId, l_ringBufsize );

                if ( l_fapirc != l_ringModifiers[i].rc )
                {
                    // note: uint32_t below is an _operator_ of fapi::ReturnCode
                    TS_FAIL( "testRepairRings: getMvpdRing rc FAIL 1: rc=0x%x,"
                             " ring=0x%X, chiplet=0x%X, i=%d",
                             static_cast<uint32_t>(l_fapirc),
                             l_ringId,
                             l_chipletId,
                             i );
                    fapiLogError(l_fapirc);
                    continue;
                }
                else if( l_fapirc != fapi::FAPI_RC_SUCCESS )
                {
                    // not an error, but the next check isn't valid
                    continue;
                }

                if ( l_ringBufsize != l_ringModifiers[i].size )
                {
                    TS_FAIL( "testRepairRings: size mismatch FAIL3 on ring %X:"
                             " exp=0x%x, act=0x%x",
                             l_ringId,
                             l_ringModifiers[i].size,
                             l_ringBufsize  );
                }

                // Dump ring buffer here.
                TRACDBIN(g_trac_test,"testRepairRings:Dump Repair Ring Buffer:",
                      l_pRingBuf,
                      l_ringBufsize );
           }

            //  ----------------------------------------------------------------
            //  write different data, read it back to verify, then put the
            //     original data back.
            //  ----------------------------------------------------------------
            uint32_t  l_offset =0;
            uint8_t   l_data = 0;
            uint8_t  *l_pData = NULL;

            // use data from last fetch test case.
            TS_TRACE( "testRepairRings: update in place  ring %d", TEST_INDEX );
            l_ringId    =   l_ringModifiers[TEST_INDEX].ringIdval;
            l_chipletId =   l_ringModifiers[TEST_INDEX].chipletIdval;
            l_ringBufsize = l_ringModifiers[TEST_INDEX].size;

            // put in recognizable data for the debug data dump
            l_pData = l_pRingBuf+sizeof(CompressedScanData);
            for (l_offset = 0,l_data=0x10; l_offset <
                 l_ringBufsize-sizeof(CompressedScanData); l_offset++)
            {
               *l_pData++ = l_data++;
            }
            TRACDBIN( g_trac_test, "testRepairRings: updated ring data:",
                      l_pRingBuf,
                      l_ringBufsize );
            setMvpdRing_FP_t (l_setMvpdRing) = &setMvpdRing; //verify typedef
            l_fapirc    =   (*l_setMvpdRing)(  MVPD_RECORD_CP00,
                                          l_ringModifiers[TEST_INDEX].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings: setMvpdRing rc FAIL 1: exp=0x%x,"
                         " rc=0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc) );
                fapiLogError(l_fapirc);
            }

            //  ----------------------------------------------------------------
            //  write back a smaller ring to cause a shift left in the record
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: shrink a  ring %d", TEST_INDEX );
            l_ringBufsize = l_ringModifiers[TEST_INDEX].size-4;
            reinterpret_cast<CompressedScanData *>(l_pRingBuf)->
                                                   iv_size = l_ringBufsize;

            // put in recognizable data for the debug data dump
            l_pData = l_pRingBuf+sizeof(CompressedScanData);
            for (l_offset = 0,l_data=0x20; l_offset <
                 l_ringBufsize-sizeof(CompressedScanData); l_offset++)
            {
               *l_pData++ = l_data++;
            }
            TRACDBIN( g_trac_test, "testRepairRings: updated ring data:",
                      l_pRingBuf,
                      l_ringBufsize );
            l_fapirc    =   setMvpdRing(  MVPD_RECORD_CP00,
                                          l_ringModifiers[TEST_INDEX].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings: setMvpdRing rc FAIL 2:"
                         " exp=0x%x, rc=0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc) );

                fapiLogError(l_fapirc);
            }

            //  ----------------------------------------------------------------
            //  write back a larger  ring to cause a shift right in the record
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: grow a ring %d", TEST_INDEX );
            l_ringBufsize = l_ringModifiers[TEST_INDEX].size+16;
            reinterpret_cast<CompressedScanData *>(l_pRingBuf)->
                                                 iv_size = l_ringBufsize;

            // put in recognizable data for the debug data dump
            l_pData = l_pRingBuf+sizeof(CompressedScanData);
            for (l_offset = 0,l_data=0x30; l_offset <
                         l_ringBufsize-sizeof(CompressedScanData); l_offset++)
            {
               *l_pData++ = l_data++;
            }
            TRACDBIN( g_trac_test, "testRepairRings: updated ring data:",
                      l_pRingBuf,
                      l_ringBufsize );
            l_fapirc    =   setMvpdRing(  MVPD_RECORD_CP00,
                                          l_ringModifiers[TEST_INDEX].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings: setMvpdRing rc FAIL 3: exp=0x%x,"
                         " rc=0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc) );

                fapiLogError(l_fapirc);
            }

            //  ----------------------------------------------------------------
            //  append a ring that is not already there to the end
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: append a ring" );
            l_ringId    =   0x77;
            l_chipletId =   0x88;
            l_ringBufsize = l_ringModifiers[TEST_INDEX].size;
            reinterpret_cast<CompressedScanData *>(l_pRingBuf)->
                                                   iv_size = l_ringBufsize;
            reinterpret_cast<CompressedScanData *>(l_pRingBuf)->
                                                   iv_ringId = l_ringId;
            reinterpret_cast<CompressedScanData *>(l_pRingBuf)->
                                                   iv_chipletId = l_chipletId;

            // put in recognizable data for the debug data dump
            l_pData = l_pRingBuf+sizeof(CompressedScanData);
            for (l_offset = 0,l_data=0x50; l_offset <
                        l_ringBufsize-sizeof(CompressedScanData); l_offset++)
            {
               *l_pData++ = l_data++;
            }
            TRACDBIN( g_trac_test, "testRepairRings: updated ring data:",
                      l_pRingBuf,
                      l_ringBufsize );
            l_fapirc    =   setMvpdRing(  MVPD_RECORD_CP00,
                                          l_ringModifiers[TEST_INDEX].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings: setMvpdRing rc FAIL 4: exp=0x%x,"
                         " rc=0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc) );

                fapiLogError(l_fapirc);
            }

            // read back data to prove the writes worked
            l_ringId    =   l_ringModifiers[TEST_INDEX].ringIdval;
            l_chipletId =   l_ringModifiers[TEST_INDEX].chipletIdval;
            l_ringBufsize = l_ringModifiers[TEST_INDEX].size+16;
            l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          l_ringModifiers[TEST_INDEX].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
            {
                TS_FAIL( "testRepairRings: getMvpdRing rc FAIL 2: 0x%x, 0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc) );
                fapiLogError(l_fapirc);
            }
            else
            {
                l_pData = l_pRingBuf+sizeof(CompressedScanData);
                for( l_offset = 0,l_data=0x30;
                     l_offset < l_ringBufsize-sizeof(CompressedScanData);
                     l_offset++)
                {
                    if( l_pData[l_offset] != l_data++ )
                    {
                        TS_FAIL("Mismatch after write on ring %X",
                                l_ringId);
                        TRACFBIN(g_trac_test,
                                 "ringdata=",
                                 l_pRingBuf,l_ringBufsize);
                    }
                }
            }

            // read back data to prove the writes worked
            l_ringId    =   0x77;
            l_chipletId =   0x88;
            l_ringBufsize = l_ringModifiers[TEST_INDEX].size;
            l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          l_ringModifiers[TEST_INDEX].keyword,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::FAPI_RC_SUCCESS )
            {
                TS_FAIL( "testRepairRings: getMvpdRing rc FAIL 3: 0x%x, 0x%x",
                         fapi::FAPI_RC_SUCCESS,
                         static_cast<uint32_t>(l_fapirc) );
                fapiLogError(l_fapirc);
            }
            else
            {
                l_pData = l_pRingBuf+sizeof(CompressedScanData);
                for (l_offset = 0,l_data=0x50;
                     l_offset < l_ringBufsize-sizeof(CompressedScanData);
                     l_offset++)
                {
                    if( l_pData[l_offset] != l_data++ )
                    {
                        TS_FAIL("Mismatch after write on ring %X",
                                l_ringId);
                        TRACFBIN(g_trac_test,
                                 "ringdata=",
                                 l_pRingBuf,l_ringBufsize);
                    }
                }
            }


            //  ----------------------------------------------------------------
            //  Pass in a buffer size that does not match the exact size
            //  of the ringId/chipletId, should  return with correct length
            //  and invalid size return code.
            //  ----------------------------------------------------------------
            TS_TRACE("testRepairRing:bad size ring=0x%x chiplet=0x%x size=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            l_ringBufsize   =   l_bufsize;
            l_ringId        =   l_ringModifiers[TEST_INDEX].ringIdval;
            l_chipletId     =   l_ringModifiers[TEST_INDEX].chipletIdval;
            l_fapirc    =   setMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::RC_MVPD_RING_FUNC_INVALID_PARAMETER )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL("testRepairRings:invalid ring size rc FAIL:"
                         " exp=0x%x, act=0x%x",
                         fapi::RC_REPAIR_RING_INVALID_SIZE,
                         static_cast<uint32_t>(l_fapirc));

                fapiLogError(l_fapirc);
            }

            //  ----------------------------------------------------------------
            //  Pass in 0 for the ring modifier, should return with "not found"
            //  error
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: pass in invalid ringId" );
            l_ringBufsize   =   l_ringModifiers[TEST_INDEX].size;
            l_ringId        =   0;     // ringId
            l_chipletId     =   0;     // chipletId
            l_fapirc    =   setMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            TS_TRACE("testRepairRings:ringId=0x%x chipletId=0x%x size=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            if ( l_fapirc != fapi::RC_MVPD_RING_FUNC_INVALID_PARAMETER )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings:  rc FAIL: exp=0x%x, act=0x%x",
                         fapi::RC_REPAIR_RING_NOT_FOUND,
                         static_cast<uint32_t>(l_fapirc) );
                fapiLogError(l_fapirc);
            }
            //  ----------------------------------------------------------------
            //  Pass in a NULL pointer with a valid ringId/chipletId, should
            //  return with correct length and successful return code.
            //  ----------------------------------------------------------------
            TS_TRACE( "testRepairRings: get size of ring(from set) %d ",
                     TEST_INDEX );
            l_ringBufsize   =   0x0;
            l_ringId        =   l_ringModifiers[TEST_INDEX].ringIdval;
            l_chipletId     =   l_ringModifiers[TEST_INDEX].chipletIdval;
            l_fapirc    =   setMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          NULL,
                                          l_ringBufsize );

            TS_TRACE("testRepairRings:ringId=0x%x chipletId=0x%x size=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            if ( l_fapirc != fapi::RC_MVPD_RING_FUNC_INVALID_PARAMETER )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL( "testRepairRings: setMvpdRing rc FAIL 5:"
                         " exp=0x%x, act=0x%x",
                         fapi::RC_MVPD_RING_FUNC_INVALID_PARAMETER,
                         static_cast<uint32_t>(l_fapirc));

                fapiLogError(l_fapirc);
            }

            //  ----------------------------------------------------------------
            //  Pass in an invalid chiplet id with a valid ring, should fail
            //  ----------------------------------------------------------------
            l_ringBufsize   =   l_bufsize;
            l_ringId        =   l_ringModifiers[TEST_INDEX].ringIdval;
            l_chipletId     =   0x22;
            TS_TRACE("testRepairRing:invalid chiplet ring=0x%X chiplet=0x%X"
                      " size=0x%x",
                      l_ringId,
                      l_chipletId,
                      l_ringBufsize );
            l_fapirc    =   getMvpdRing(  MVPD_RECORD_CP00,
                                          MVPD_KEYWORD_PDR,
                                          l_fapi_cpu_target,
                                          l_chipletId,
                                          l_ringId,
                                          l_pRingBuf,
                                          l_ringBufsize );

            if ( l_fapirc != fapi::RC_REPAIR_RING_NOT_FOUND )
            {
                //  note:  "uint32_t" below is an _operator_ of fapi::ReturnCode
                TS_FAIL("testRepairRings:invalid chipletid rc FAIL:"
                         " exp=0x%x, act=0x%x",
                         fapi::RC_REPAIR_RING_NOT_FOUND,
                         static_cast<uint32_t>(l_fapirc));

                fapiLogError(l_fapirc);
            }

        }

        //-- Put the original data back into the vpd
        for( std::list<saveRestoreData_t>::iterator sv = l_srData.begin();
             sv != l_srData.end();
             ++sv )
        {
            if( sv->target == NULL )
            {
                continue;
            }

            if( sv->CP00_pdG != NULL )
            {
                l_errhdl = deviceWrite( sv->target,
                                        sv->CP00_pdG,
                                        sv->CP00_pdG_size,
                                        DEVICE_MVPD_ADDRESS( MVPD_RECORD_CP00,
                                                          MVPD_KEYWORD_PDG ) );
                if( l_errhdl )
                {
                    TS_FAIL("Error restoring CP00/#G to %.8X",
                            TARGETING::get_huid(sv->target));
                    errlCommit( l_errhdl, VPD_COMP_ID );
                }
                delete[] sv->CP00_pdG;
                sv->CP00_pdG = NULL;
            }

            if( sv->CP00_pdR != NULL )
            {
                l_errhdl = deviceWrite( sv->target,
                                        sv->CP00_pdR,
                                        sv->CP00_pdR_size,
                                        DEVICE_MVPD_ADDRESS( MVPD_RECORD_CP00,
                                                          MVPD_KEYWORD_PDR ) );
                if( l_errhdl )
                {
                    TS_FAIL("Error restoring CP00/#R to %.8X",
                            TARGETING::get_huid(sv->target));
                    errlCommit( l_errhdl, VPD_COMP_ID );
                }
                delete[] sv->CP00_pdR;
                sv->CP00_pdR = NULL;
            }
        }
        //--

        //  delete allocated space
        if( l_pRingBuf )
        {
            delete[]    l_pRingBuf;
        }
        for( std::list<saveRestoreData_t>::iterator sv = l_srData.begin();
             sv != l_srData.end();
             ++sv )
        {
            if( sv->CP00_pdG != NULL )
            {
                delete[] sv->CP00_pdG;
                sv->CP00_pdG = NULL;
            }
            if( sv->CP00_pdR != NULL )
            {
                delete[] sv->CP00_pdR;
                sv->CP00_pdR = NULL;
            }
        }


        TS_TRACE( "testRepairRings exit" );
    }

};  // end class

#endif
