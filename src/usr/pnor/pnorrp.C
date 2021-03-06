/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/pnor/pnorrp.C $                                       */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2011,2015                        */
/* [+] Google Inc.                                                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
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
#include "pnorrp.H"
#include <pnor/pnor_reasoncodes.H>
#include <initservice/taskargs.H>
#include <sys/msg.h>
#include <trace/interface.H>
#include <errl/errlmanager.H>
#include <targeting/common/targetservice.H>
#include <devicefw/userif.H>
#include <limits.h>
#include <string.h>
#include <sys/mm.h>
#include <errno.h>
#include <initservice/initserviceif.H>
#include "pnordd.H"
#include "ffs.h"   //Common header file with BuildingBlock.
#include "common/ffs_hb.H"//Hostboot definition of user data in ffs_entry struct
#include <pnor/ecc.H>
#include <kernel/console.H>
#include <endian.h>
#include <util/align.H>
#include <config.h>
#include "pnor_common.H"


extern trace_desc_t* g_trac_pnor;

// Easy macro replace for unit testing
//#define TRACUCOMP(args...)  TRACFCOMP(args)
#define TRACUCOMP(args...)

using namespace PNOR;

/**
 * Eyecatcher strings for PNOR TOC entries
 */
extern const char* cv_EYECATCHER[];

/**
 * @brief   set up _start() task entry procedure for PNOR daemon
 */
TASK_ENTRY_MACRO( PnorRP::init );


/********************
 Public Methods
 ********************/

/**
 * @brief  Return the size and address of a given section of PNOR data
 */
errlHndl_t PNOR::getSectionInfo( PNOR::SectionId i_section,
                                 PNOR::SectionInfo_t& o_info )
{
    return Singleton<PnorRP>::instance().getSectionInfo(i_section,o_info);
}

/**
 * @brief  Clear pnor section
 */
errlHndl_t PNOR::clearSection(PNOR::SectionId i_section)
{
    return Singleton<PnorRP>::instance().clearSection(i_section);
}

/**
 * @brief  Write the data for a given sectino into PNOR
 */
errlHndl_t PNOR::flush( PNOR::SectionId i_section)
{
    errlHndl_t l_err = NULL;
    do {
        PNOR::SectionInfo_t l_info;
        l_err = getSectionInfo(i_section, l_info);
        if (l_err)
        {
            TRACFCOMP(g_trac_pnor, "PNOR::flush: getSectionInfo errored,"
                    " secId: %d", (int)i_section);
            break;
        }
        int l_rc = mm_remove_pages (RELEASE,
                reinterpret_cast<void*>(l_info.vaddr), l_info.size);
        if (l_rc)
        {
            TRACFCOMP(g_trac_pnor, "PNOR::flush: mm_remove_pages errored,"
                    " secId: %d, rc: %d", (int)i_section, l_rc);
            /*@
             *  @errortype
             *  @moduleid       PNOR::MOD_PNORRP_FLUSH
             *  @reasoncode     PNOR::RC_MM_REMOVE_PAGES_FAILED
             *  @userdata1      section Id
             *  @userdata2      RC
             *  @devdesc        mm_remove_pages failed
             */
            l_err = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                        PNOR::MOD_PNORRP_FLUSH,
                                        PNOR::RC_MM_REMOVE_PAGES_FAILED,
                                        i_section, l_rc, true);
            break;
        }
    } while (0);
    return l_err;
}

/**
 * @brief  check and fix correctable ECC for a given pnor section
 */
errlHndl_t PNOR::fixECC(PNOR::SectionId i_section)
{
    return Singleton<PnorRP>::instance().fixECC(i_section);
}

/**
 * STATIC
 * @brief Static Initializer
 */
void PnorRP::init( errlHndl_t   &io_rtaskRetErrl )
{
    TRACUCOMP(g_trac_pnor, "PnorRP::init> " );
    uint64_t rc = 0;
    errlHndl_t  l_errl  =   NULL;

    if( Singleton<PnorRP>::instance().didStartupFail(rc) )
    {
        /*@
         *  @errortype      ERRL_SEV_CRITICAL_SYS_TERM
         *  @moduleid       PNOR::MOD_PNORRP_DIDSTARTUPFAIL
         *  @reasoncode     PNOR::RC_BAD_STARTUP_RC
         *  @userdata1      return code
         *  @userdata2      0
         *
         *  @devdesc        PNOR startup task returned an error.
         * @custdesc    A problem occurred while accessing the boot flash.
         */
        l_errl = new ERRORLOG::ErrlEntry(
                                ERRORLOG::ERRL_SEV_CRITICAL_SYS_TERM,
                                PNOR::MOD_PNORRP_DIDSTARTUPFAIL,
                                PNOR::RC_BAD_STARTUP_RC,
                                rc,
                                0,
                                true /*Add HB SW Callout*/ );

        l_errl->collectTrace(PNOR_COMP_NAME);
    }

    io_rtaskRetErrl=l_errl;
}


/********************
 Helper Methods
 ********************/

/**
 * @brief  Static function wrapper to pass into task_create
 */
void* wait_for_message( void* unused )
{
    TRACUCOMP(g_trac_pnor, "wait_for_message> " );
    Singleton<PnorRP>::instance().waitForMessage();
    return NULL;
}

/********************
 Private/Protected Methods
 ********************/

/**
 * @brief  Constructor
 */
PnorRP::PnorRP()
: iv_activeTocOffsets(SIDE_A_TOC_0_OFFSET,SIDE_A_TOC_1_OFFSET)
,iv_altTocOffsets(SIDE_B_TOC_0_OFFSET,SIDE_B_TOC_1_OFFSET)
,iv_TOC_used(TOC_0)
,iv_msgQ(NULL)
,iv_startupRC(0)
{
    TRACFCOMP(g_trac_pnor, "PnorRP::PnorRP> " );
    // setup everything in a separate function
    initDaemon();

    TRACFCOMP(g_trac_pnor, "< PnorRP::PnorRP : Startup Errors=%X ", iv_startupRC );
}

/**
 * @brief  Destructor
 */
PnorRP::~PnorRP()
{
    TRACFCOMP(g_trac_pnor, "PnorRP::~PnorRP> " );

    // delete the message queue we created
    if( iv_msgQ )
    {
        msg_q_destroy( iv_msgQ );
    }

    // should kill the task we spawned, but that isn't needed right now

    TRACFCOMP(g_trac_pnor, "< PnorRP::~PnorRP" );
}

/**
 * @brief Initialize the daemon
 */
void PnorRP::initDaemon()
{
    TRACUCOMP(g_trac_pnor, "PnorRP::initDaemon> " );
    errlHndl_t l_errhdl = NULL;

    do
    {
        // @TODO RTC: 120062 - Determine which side is Golden
        // Default TOC offsets set to side A. If two side support is enabled,
        // check which SEEPROM hostboot booted from
#ifdef CONFIG_TWO_SIDE_SUPPORT
        TARGETING::Target* pnor_target = TARGETING::
                                         MASTER_PROCESSOR_CHIP_TARGET_SENTINEL;
        // Get correct TOC
        PNOR::sbeSeepromSide_t l_bootSide;
        PNOR::getSbeBootSeeprom(pnor_target, l_bootSide);
        if (l_bootSide == PNOR::SBE_SEEPROM1)
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::initDaemon> Booting from Side B");
            iv_activeTocOffsets.first = SIDE_B_TOC_0_OFFSET;
            iv_activeTocOffsets.second = SIDE_B_TOC_1_OFFSET;
            iv_altTocOffsets.first = SIDE_A_TOC_0_OFFSET;
            iv_altTocOffsets.second = SIDE_A_TOC_0_OFFSET;
        }
        else
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::initDaemon> Booting from Side A");
        }
#endif

        // create a message queue
        iv_msgQ = msg_q_create();

        // create a Block, passing in the message queue
        int rc = mm_alloc_block( iv_msgQ, (void*) BASE_VADDR, TOTAL_SIZE );
        if( rc )
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::initDaemon> Error from mm_alloc_block : rc=%d", rc );
            /*@
             * @errortype
             * @moduleid     PNOR::MOD_PNORRP_INITDAEMON
             * @reasoncode   PNOR::RC_EXTERNAL_ERROR
             * @userdata1    Requested Address
             * @userdata2    rc from mm_alloc_block
             * @devdesc      PnorRP::initDaemon> Error from mm_alloc_block
             * @custdesc     A problem occurred while accessing the boot flash.
             */
            l_errhdl = new ERRORLOG::ErrlEntry(
                           ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                           PNOR::MOD_PNORRP_INITDAEMON,
                           PNOR::RC_EXTERNAL_ERROR,
                           TO_UINT64(BASE_VADDR),
                           TO_UINT64(rc),
                           true /*Add HB SW Callout*/);
            l_errhdl->collectTrace(PNOR_COMP_NAME);
            break;
        }

        //Register this memory range to be FLUSHed during a shutdown.
        INITSERVICE::registerBlock(reinterpret_cast<void*>(BASE_VADDR),
                                   TOTAL_SIZE,PNOR_PRIORITY);

        // Read the TOC in the PNOR to compute the sections and set their
        // correct permissions
        l_errhdl = readTOC();
        if( l_errhdl )
        {
            TRACFCOMP(g_trac_pnor, ERR_MRK"PnorRP::initDaemon: Failed to readTOC");
            break;
        }

        // start task to wait on the queue
        task_create( wait_for_message, NULL );

    } while(0);

    if( l_errhdl )
    {
        iv_startupRC = l_errhdl->reasonCode();
        errlCommit(l_errhdl,PNOR_COMP_ID);
    }

// Not supporting PNOR error in VPO
#ifndef CONFIG_VPO_COMPILE
    // call ErrlManager function - tell him that PNOR is ready!
    ERRORLOG::ErrlManager::errlResourceReady(ERRORLOG::PNOR);
#endif

    TRACUCOMP(g_trac_pnor, "< PnorRP::initDaemon" );
}


/**
 * @brief  Return the size and address of a given section of PNOR data
 */
errlHndl_t PnorRP::getSectionInfo( PNOR::SectionId i_section,
                                   PNOR::SectionInfo_t& o_info )
{
    //TRACDCOMP(g_trac_pnor, "PnorRP::getSectionInfo> i_section=%d", i_section );
    errlHndl_t l_errhdl = NULL;
    PNOR::SectionId id = i_section;

    do
    {
        // Abort this operation if we had a startup failure
        uint64_t rc = 0;
        if( didStartupFail(rc) )
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::getSectionInfo> RP not properly initialized, failing : rc=%X", rc );
            /*@
             * @errortype
             * @moduleid     PNOR::MOD_PNORRP_GETSECTIONINFO
             * @reasoncode   PNOR::RC_STARTUP_FAIL
             * @userdata1    Requested Section
             * @userdata2    Startup RC
             * @devdesc      PnorRP::getSectionInfo> RP not properly initialized
             * @custdesc     A problem occurred while accessing the boot flash.
             */
            l_errhdl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                               PNOR::MOD_PNORRP_GETSECTIONINFO,
                                               PNOR::RC_STARTUP_FAIL,
                                               TO_UINT64(i_section),
                                               rc,
                                               true /*Add HB SW Callout*/);
            l_errhdl->collectTrace(PNOR_COMP_NAME);

            // set the return section to our invalid data
            id = PNOR::INVALID_SECTION;
            break;
        }

        // Zero-length means the section is invalid
        if( 0 == iv_TOC[id].size )
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::getSectionInfo> Invalid Section Requested : i_section=%d", i_section );
            TRACFCOMP(g_trac_pnor, "o_info={ id=%d, size=%d }", iv_TOC[i_section].id, iv_TOC[i_section].size );
            /*@
             * @errortype
             * @moduleid     PNOR::MOD_PNORRP_GETSECTIONINFO
             * @reasoncode   PNOR::RC_INVALID_SECTION
             * @userdata1    Requested Section
             * @userdata2    TOC used
             * @devdesc      PnorRP::getSectionInfo> Invalid Address for read/write
             * @custdesc     A problem occurred while accessing the boot flash.
            */
            l_errhdl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                               PNOR::MOD_PNORRP_GETSECTIONINFO,
                                               PNOR::RC_INVALID_SECTION,
                                               TO_UINT64(i_section),
                                               iv_TOC_used,
                                               true /*Add HB SW Callout*/);
            l_errhdl->collectTrace(PNOR_COMP_NAME);

            // set the return section to our invalid data
            id = PNOR::INVALID_SECTION;
            break;
        }
    } while(0);

    if (PNOR::INVALID_SECTION != id)
    {
        TRACDCOMP( g_trac_pnor, "PnorRP::getSectionInfo: i_section=%d, id=%d", i_section, iv_TOC[i_section].id );

        // copy my data into the external format
        o_info.id = iv_TOC[id].id;
        o_info.name = cv_EYECATCHER[id];
        o_info.vaddr = iv_TOC[id].virtAddr;
        o_info.flashAddr = iv_TOC[id].flashAddr;
        o_info.size = iv_TOC[id].size;
        o_info.eccProtected = ((iv_TOC[id].integrity & FFS_INTEG_ECC_PROTECT)
                                != 0) ? true : false;
        o_info.sha512Version = ((iv_TOC[id].version & FFS_VERS_SHA512)
                                 != 0) ? true : false;
        o_info.sha512perEC = ((iv_TOC[id].version & FFS_VERS_SHA512_PER_EC)
                               != 0) ? true : false;
        o_info.readOnly = ((iv_TOC[id].misc & FFS_MISC_READ_ONLY)
                               != 0) ? true : false;
    }

    return l_errhdl;
}

/**
 * @brief Read the TOC and store section information
 */
errlHndl_t PnorRP::readTOC()
{
    TRACUCOMP(g_trac_pnor, "PnorRP::readTOC>" );
    errlHndl_t l_errhdl = NULL;
    uint8_t* toc0Buffer = new uint8_t[PAGESIZE];
    uint8_t* toc1Buffer = new uint8_t[PAGESIZE];
    uint64_t fatal_error = 0;
    do {
        l_errhdl = readFromDevice( iv_activeTocOffsets.first, 0, false,
                                toc0Buffer, fatal_error );
        if (l_errhdl)
        {
            TRACFCOMP(g_trac_pnor, "readTOC: readFromDevice failed for TOC0");
            break;
        }

        l_errhdl = readFromDevice( iv_activeTocOffsets.second, 0, false,
                                   toc1Buffer, fatal_error );
        if (l_errhdl)
        {
            TRACFCOMP(g_trac_pnor, "readTOC: readFromDevice failed for TOC1");
            break;
        }

        l_errhdl = PNOR::parseTOC(toc0Buffer, toc1Buffer, iv_TOC_used, iv_TOC,
                                  BASE_VADDR);
        if (l_errhdl)
        {
            TRACFCOMP(g_trac_pnor, "readTOC: parseTOC failed");
            errlCommit(l_errhdl, PNOR_COMP_ID);
            INITSERVICE::doShutdown(PNOR::RC_PARTITION_TABLE_INVALID);
        }
    } while (0);

    if(toc0Buffer != NULL)
    {
        delete[] toc0Buffer;
    }

    if(toc1Buffer != NULL)
    {
        delete[] toc1Buffer;
    }
    TRACUCOMP(g_trac_pnor, "< PnorRP::readTOC" );
    return l_errhdl;
}

/**
 * @brief  Message receiver
 */
void PnorRP::waitForMessage()
{
    TRACFCOMP(g_trac_pnor, "PnorRP::waitForMessage>" );

    errlHndl_t l_errhdl = NULL;
    msg_t* message = NULL;
    uint8_t* user_addr = NULL;
    uint8_t* eff_addr = NULL;
    uint64_t dev_offset = 0;
    uint64_t chip_select = 0xF;
    bool needs_ecc = false;
    int rc = 0;
    uint64_t status_rc = 0;
    uint64_t fatal_error = 0;

    while(1)
    {
        status_rc = 0;
        TRACUCOMP(g_trac_pnor, "PnorRP::waitForMessage> waiting for message" );
        message = msg_wait( iv_msgQ );
        if( message )
        {
            /*  data[0] = virtual address requested
             *  data[1] = address to place contents
             */
            eff_addr = (uint8_t*)message->data[0];
            user_addr = (uint8_t*)message->data[1];

            //figure out the real pnor offset
            l_errhdl = computeDeviceAddr( eff_addr, dev_offset, chip_select, needs_ecc );
            if( l_errhdl )
            {
                status_rc = -EFAULT; /* Bad address */
            }
            else
            {
                switch(message->type)
                {
                    case( MSG_MM_RP_READ ):
                        l_errhdl = readFromDevice( dev_offset,
                                                   chip_select,
                                                   needs_ecc,
                                                   user_addr,
                                                   fatal_error );
                        if( l_errhdl || ( 0 != fatal_error ) )
                        {
                            status_rc = -EIO; /* I/O error */
                        }
                        break;
                    case( MSG_MM_RP_WRITE ):
                        l_errhdl = writeToDevice( dev_offset, chip_select, needs_ecc, user_addr );
                        if( l_errhdl )
                        {
                            status_rc = -EIO; /* I/O error */
                        }
                        break;
                    default:
                        TRACFCOMP( g_trac_pnor, "PnorRP::waitForMessage> Unrecognized message type : user_addr=%p, eff_addr=%p, msgtype=%d", user_addr, eff_addr, message->type );
                        /*@
                         * @errortype
                         * @moduleid     PNOR::MOD_PNORRP_WAITFORMESSAGE
                         * @reasoncode   PNOR::RC_INVALID_MESSAGE_TYPE
                         * @userdata1    Message type
                         * @userdata2    Requested Virtual Address
                         * @devdesc      PnorRP::waitForMessage> Unrecognized
                         *               message type
                         * @custdesc     A problem occurred while accessing
                         *               the boot flash.
                         */
                        l_errhdl = new ERRORLOG::ErrlEntry(
                                           ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                           PNOR::MOD_PNORRP_WAITFORMESSAGE,
                                           PNOR::RC_INVALID_MESSAGE_TYPE,
                                           TO_UINT64(message->type),
                                           (uint64_t)eff_addr,
                                           true /*Add HB SW Callout*/);
                        l_errhdl->collectTrace(PNOR_COMP_NAME);
                        status_rc = -EINVAL; /* Invalid argument */
                }
            }

            if( !l_errhdl && msg_is_async(message) )
            {
                TRACFCOMP( g_trac_pnor, "PnorRP::waitForMessage> Unsupported Asynchronous Message  : user_addr=%p, eff_addr=%p, msgtype=%d", user_addr, eff_addr, message->type );
                /*@
                 * @errortype
                 * @moduleid     PNOR::MOD_PNORRP_WAITFORMESSAGE
                 * @reasoncode   PNOR::RC_INVALID_ASYNC_MESSAGE
                 * @userdata1    Message type
                 * @userdata2    Requested Virtual Address
                 * @devdesc      PnorRP::waitForMessage> Unrecognized message
                 *               type
                 * @custdesc     A problem occurred while accessing the boot
                 *               flash.
                 */
                l_errhdl = new ERRORLOG::ErrlEntry(
                                         ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                         PNOR::MOD_PNORRP_WAITFORMESSAGE,
                                         PNOR::RC_INVALID_ASYNC_MESSAGE,
                                         TO_UINT64(message->type),
                                         (uint64_t)eff_addr,
                                         true /*Add HB SW Callout*/);
                l_errhdl->collectTrace(PNOR_COMP_NAME);
                status_rc = -EINVAL; /* Invalid argument */
            }

            if( l_errhdl )
            {
                errlCommit(l_errhdl,PNOR_COMP_ID);
            }


            /*  Expected Response:
             *      data[0] = virtual address requested
             *      data[1] = rc (0 or negative errno value)
             *      extra_data = Specific reason code.
             */
            message->data[1] = status_rc;
            message->extra_data = reinterpret_cast<void*>(fatal_error);
            rc = msg_respond( iv_msgQ, message );
            if( rc )
            {
                TRACFCOMP(g_trac_pnor, "PnorRP::waitForMessage> Error from msg_respond, giving up : rc=%d", rc );
                break;
            }
        }
    }


    TRACFCOMP(g_trac_pnor, "< PnorRP::waitForMessage" );
}


/**
 * @brief  Retrieve 1 page of data from the PNOR device
 */
errlHndl_t PnorRP::readFromDevice( uint64_t i_offset,
                                   uint64_t i_chip,
                                   bool i_ecc,
                                   void* o_dest,
                                   uint64_t& o_fatalError )
{
    TRACUCOMP(g_trac_pnor, "PnorRP::readFromDevice> i_offset=0x%X, i_chip=%d", i_offset, i_chip );
    errlHndl_t l_errhdl = NULL;
    uint8_t* ecc_buffer = NULL;
    o_fatalError = 0;

    do
    {
        TARGETING::Target* pnor_target = TARGETING::MASTER_PROCESSOR_CHIP_TARGET_SENTINEL;

        // assume a single page
        void* data_to_read = o_dest;
        size_t read_size = PAGESIZE;

        // if we need to handle ECC we need to read more than 1 page
        if( i_ecc )
        {
            ecc_buffer = new uint8_t[PAGESIZE_PLUS_ECC]();
            data_to_read = ecc_buffer;
            read_size = PAGESIZE_PLUS_ECC;
        }

        // get the data from the PNOR DD
        l_errhdl = DeviceFW::deviceRead(pnor_target,
                                        data_to_read,
                                        read_size,
                                        DEVICE_PNOR_ADDRESS(i_chip,i_offset) );
        if( l_errhdl )
        {
            TRACFCOMP(g_trac_pnor, "PnorRP::readFromDevice> Error from device : RC=%X", l_errhdl->reasonCode() );
            break;
        }

        // remove the ECC data
        if( i_ecc )
        {
            // remove the ECC and fix the original data if it is broken
            PNOR::ECC::eccStatus ecc_stat =
              PNOR::ECC::removeECC( reinterpret_cast<uint8_t*>(data_to_read),
                                    reinterpret_cast<uint8_t*>(o_dest),
                                    PAGESIZE );

            // create an error if we couldn't correct things
            if( ecc_stat == PNOR::ECC::UNCORRECTABLE )
            {
                TRACFCOMP( g_trac_pnor, "PnorRP::readFromDevice> Uncorrectable ECC error : chip=%d,offset=0x%.X", i_chip, i_offset );

                // Need to shutdown here instead of creating an error log
                //  because the bad page could be critical to the regular
                //  error handling path and cause an infinite loop.
                // Also need to spawn a separate task to do the shutdown
                //  so that the regular PNOR task can service the writes
                //  that happen during shutdown.
                o_fatalError = PNOR::RC_ECC_UE;
                INITSERVICE::doShutdown( PNOR::RC_ECC_UE, true );
            }
            // found an error so we need to fix something
            else if( ecc_stat != PNOR::ECC::CLEAN )
            {
                TRACFCOMP( g_trac_pnor, "PnorRP::readFromDevice> Correctable ECC error : chip=%d, offset=0x%.X", i_chip, i_offset );

                // need to write good data back to PNOR
                l_errhdl = DeviceFW::deviceWrite(pnor_target,
                                       data_to_read,//corrected data
                                       read_size,
                                       DEVICE_PNOR_ADDRESS(i_chip,i_offset) );
                if( l_errhdl )
                {
                    TRACFCOMP(g_trac_pnor, "PnorRP::readFromDevice> Error writing corrected data back to device : RC=%X", l_errhdl->reasonCode() );
                    // we don't need to fail here since we can correct
                    //  it the next time we read it again, instead just
                    //  commit the log here
                    errlCommit(l_errhdl,PNOR_COMP_ID);
                }

                // keep some stats here in case we want them someday
                //no need for mutex since only ever 1 thread accessing this
                iv_stats[i_offset/PAGESIZE].numCEs++;
            }
        }
    } while(0);

    if( ecc_buffer )
    {
        delete[] ecc_buffer;
    }

    TRACUCOMP(g_trac_pnor, "< PnorRP::readFromDevice" );
    return l_errhdl;
}

/**
 * @brief  Write 1 page of data to the PNOR device
 */
errlHndl_t PnorRP::writeToDevice( uint64_t i_offset,
                                  uint64_t i_chip,
                                  bool i_ecc,
                                  void* i_src )
{
    TRACUCOMP(g_trac_pnor, "PnorRP::writeToDevice> i_offset=%X, i_chip=%d", i_offset, i_chip );
    errlHndl_t l_errhdl = NULL;
    uint8_t* ecc_buffer = NULL;

    do
    {
        TARGETING::Target* pnor_target = TARGETING::MASTER_PROCESSOR_CHIP_TARGET_SENTINEL;

        // assume a single page to write
        void* data_to_write = i_src;
        size_t write_size = PAGESIZE;

        // apply ECC to data if needed
        if( i_ecc )
        {
            ecc_buffer = new uint8_t[PAGESIZE_PLUS_ECC];
            PNOR::ECC::injectECC( reinterpret_cast<uint8_t*>(i_src),
                                  PAGESIZE,
                                  reinterpret_cast<uint8_t*>(ecc_buffer) );
            data_to_write = reinterpret_cast<void*>(ecc_buffer);
            write_size = PAGESIZE_PLUS_ECC;
        }

        //no need for mutex since only ever a singleton object
        iv_stats[i_offset/PAGESIZE].numWrites++;

        // write the data out to the PNOR DD
        errlHndl_t l_errhdl = DeviceFW::deviceWrite( pnor_target,
                                       data_to_write,
                                       write_size,
                                       DEVICE_PNOR_ADDRESS(i_chip,i_offset) );
        if( l_errhdl )
        {
            TRACFCOMP(g_trac_pnor, "PnorRP::writeToDevice> Error from device : RC=%X", l_errhdl->reasonCode() );
            break;
        }
    } while(0);

    if( ecc_buffer )
    {
        delete[] ecc_buffer;
    }

    TRACUCOMP(g_trac_pnor, "< PnorRP::writeToDevice" );
    return l_errhdl;
}

/**
 * @brief  Convert a virtual address into the PNOR device address
 */
errlHndl_t PnorRP::computeDeviceAddr( void* i_vaddr,
                                      uint64_t& o_offset,
                                      uint64_t& o_chip,
                                      bool& o_ecc )
{
    errlHndl_t l_errhdl = NULL;
    o_offset = 0;
    o_chip = 99;
    uint64_t l_vaddr = (uint64_t)i_vaddr;

    do
    {
        // make sure this is one of our addresses
        if( !((l_vaddr >= BASE_VADDR)
              && (l_vaddr < LAST_VADDR)) )
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::computeDeviceAddr> Virtual Address outside known PNOR range : i_vaddr=%p", i_vaddr );
            /*@
             * @errortype
             * @moduleid     PNOR::MOD_PNORRP_WAITFORMESSAGE
             * @reasoncode   PNOR::RC_INVALID_ADDRESS
             * @userdata1    Virtual Address
             * @userdata2    Base PNOR Address
             * @devdesc      PnorRP::computeDeviceAddr> Virtual Address outside
             *               known PNOR range
             * @custdesc    A problem occurred while accessing the boot flash.
             */
            l_errhdl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                            PNOR::MOD_PNORRP_COMPUTEDEVICEADDR,
                                            PNOR::RC_INVALID_ADDRESS,
                                            l_vaddr,
                                            BASE_VADDR,
                                            true /*Add HB SW Callout*/);
            l_errhdl->collectTrace(PNOR_COMP_NAME);
            break;
        }

        // find the matching section
        PNOR::SectionId id = PNOR::INVALID_SECTION;
        l_errhdl = computeSection( l_vaddr, id );
        if( l_errhdl )
        {
            TRACFCOMP( g_trac_pnor, "PnorRP::computeDeviceAddr> Virtual address does not match any pnor sections : i_vaddr=%p", i_vaddr );
            break;
        }

        // pull out the information we need to return from our global copy
        o_chip = iv_TOC[id].chip;
        o_ecc = (bool)(iv_TOC[id].integrity & FFS_INTEG_ECC_PROTECT);
        o_offset = l_vaddr - iv_TOC[id].virtAddr; //offset into section

        // for ECC we need to figure out where the ECC-enhanced offset is
        //  before tacking on the offset to the section
        if( o_ecc )
        {
            o_offset = (o_offset * 9) / 8;
        }
        // add on the offset of the section itself
        o_offset += iv_TOC[id].flashAddr;
    } while(0);

    TRACUCOMP( g_trac_pnor, "< PnorRP::computeDeviceAddr: i_vaddr=%X, o_offset=0x%X, o_chip=%d", l_vaddr, o_offset, o_chip );
    return l_errhdl;
}

/**
 * @brief Static instance function
 */
PnorRP& PnorRP::getInstance()
{
    return Singleton<PnorRP>::instance();
}

/**
 * @brief  Figure out which section a VA belongs to
 */
errlHndl_t PnorRP::computeSection( uint64_t i_vaddr,
                                   PNOR::SectionId& o_id )
{
    errlHndl_t errhdl = NULL;

    o_id = PNOR::INVALID_SECTION;

    do {
        // loop through all sections to find a matching id
        for( PNOR::SectionId id = PNOR::FIRST_SECTION;
             id < PNOR::NUM_SECTIONS;
             id = (PNOR::SectionId) (id + 1) )
        {
            if( (i_vaddr >= iv_TOC[id].virtAddr)
                && (i_vaddr < (iv_TOC[id].virtAddr + iv_TOC[id].size)) )
            {
                o_id = iv_TOC[id].id;
                break;
            }
        }

    }while(0);

    if(o_id == PNOR::INVALID_SECTION)
    {
        TRACFCOMP( g_trac_pnor, "PnorRP::computeSection> Invalid virtual address : i_vaddr=%X", i_vaddr );
        /*@
         * @errortype
         * @moduleid     PNOR::MOD_PNORRP_COMPUTESECTION
         * @reasoncode   PNOR::RC_INVALID_ADDRESS
         * @userdata1    Requested Virtual Address
         * @userdata2    <unused>
         * @devdesc      PnorRP::computeSection> Invalid Address
         * @custdesc    A problem occurred while accessing the boot flash.
         */
        errhdl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                         PNOR::MOD_PNORRP_COMPUTESECTION,
                                         PNOR::RC_INVALID_ADDRESS,
                                         i_vaddr,
                                         0,
                                         true /*Add HB SW Callout*/);
        errhdl->collectTrace(PNOR_COMP_NAME);
        return errhdl;
    }

    return errhdl;
}

errlHndl_t PnorRP::clearSection(PNOR::SectionId i_section)
{
    TRACFCOMP(g_trac_pnor, "PnorRP::clearSection Section id = %d", i_section);
    errlHndl_t l_errl = NULL;
    const uint64_t CLEAR_BYTE = 0xFF;
    uint8_t* l_buf = new uint8_t[PAGESIZE];
    uint8_t* l_eccBuf = NULL;

    do
    {
        // Flush pages of pnor section we are trying to clear
        l_errl = flush(i_section);
        if (l_errl)
        {
            TRACFCOMP( g_trac_pnor, ERR_MRK"PnorRP::clearSection: flush() failed on section",
                        i_section);
            break;
        }

        // Get PNOR section info
        uint64_t l_address = iv_TOC[i_section].flashAddr;
        uint64_t l_chipSelect = iv_TOC[i_section].chip;
        uint32_t l_size = iv_TOC[i_section].size;
        bool l_ecc = iv_TOC[i_section].integrity & FFS_INTEG_ECC_PROTECT;

        // Number of pages needed to cycle proper ECC
        // Meaning every 9th page will copy the l_eccBuf at offset 0
        const uint64_t l_eccCycleNum = 9;

        // Boundaries for properly splitting up an ECC page for 4K writes.
        // Subtract 1 from l_eccCycleNum because we start writing with offset 0
        // and add this value 8 times to complete a cycle.
        const uint64_t l_sizeOfOverlapSection = (PAGESIZE_PLUS_ECC - PAGESIZE) /
                                                (l_eccCycleNum - 1);

        // Create clear section buffer
        memset(l_buf, CLEAR_BYTE, PAGESIZE);

        // apply ECC to data if needed
        if(l_ecc)
        {
            l_eccBuf = new uint8_t[PAGESIZE_PLUS_ECC];
            PNOR::ECC::injectECC( reinterpret_cast<uint8_t*>(l_buf),
                                  PAGESIZE,
                                  reinterpret_cast<uint8_t*>(l_eccBuf) );
            l_size = (l_size*9)/8;
        }

        // Write clear section page to PNOR
        for (uint64_t i = 0; i < l_size; i+=PAGESIZE)
        {
            if(l_ecc)
            {
                // Take (current page) mod (l_eccCycleNum) to get cycle position
                uint8_t l_bufPos = ( (i/PAGESIZE) % l_eccCycleNum );
                uint64_t l_bufOffset = l_sizeOfOverlapSection * l_bufPos;
                memcpy(l_buf, (l_eccBuf + l_bufOffset), PAGESIZE);
            }

            // Set ecc parameter to false to avoid double writes will only write
            // 4k at a time, even if the section is ecc protected.
            l_errl = writeToDevice((l_address + i), l_chipSelect,
                                   false, l_buf);
            if (l_errl)
            {
                TRACFCOMP( g_trac_pnor, ERR_MRK"PnorRP::clearSection: writeToDevice fail: eid=0x%X, rc=0x%X",
                           l_errl->eid(), l_errl->reasonCode());
                break;
            }
        }
        if (l_errl)
        {
            break;
        }
    } while(0);

    // Free allocated memory
    if(l_eccBuf)
    {
        delete[] l_eccBuf;
    }
    delete [] l_buf;

    return l_errl;
}

/**
 * @brief check and fix correctable ECC errors for a given section
 */
errlHndl_t PnorRP::fixECC (PNOR::SectionId i_section)
{
    errlHndl_t l_err  = NULL;
    uint8_t* l_buffer = new uint8_t [PAGESIZE] ();
    do {
        TRACFCOMP(g_trac_pnor, ENTER_MRK"PnorRP::fixECC");

        //get info from the TOC
        uint8_t* l_virtAddr = reinterpret_cast<uint8_t*>
                                (iv_TOC[i_section].virtAddr);
        uint32_t l_size    = iv_TOC[i_section].size;
        bool l_ecc         = iv_TOC[i_section].integrity&FFS_INTEG_ECC_PROTECT;

        if (!l_ecc)
        {
            TRACFCOMP(g_trac_pnor, "PnorRP::fixECC: section is not"
                    " ecc protected");
            /*@
             *  @errortype      ERRL_SEV_INFORMATIONAL
             *  @moduleid       PNOR::MOD_PNORRP_FIXECC
             *  @reasoncode     PNOR::RC_NON_ECC_PROTECTED_SECTION
             *  @userdata1      Section ID
             *  @userdata2      0
             *
             *  @devdesc        Non ECC protected section is passed to fixECC
             */
            l_err = new ERRORLOG::ErrlEntry(
                                    ERRORLOG::ERRL_SEV_INFORMATIONAL,
                                    PNOR::MOD_PNORRP_FIXECC,
                                    PNOR::RC_NON_ECC_PROTECTED_SECTION,
                                    i_section,
                                    0,true);
            break;
        }

        uint32_t l_numOfPages = (l_size)/PAGESIZE;

        //loop over number of pages in a section
        for (uint32_t i = 0; i < l_numOfPages; i++)
        {
            TRACDCOMP(g_trac_pnor, "PnorRP::fixECC: memcpy virtAddr:0x%X",
                      l_virtAddr);
            memcpy(l_buffer, l_virtAddr, PAGESIZE);
            l_virtAddr += PAGESIZE;
        }
    } while (0);

    delete [] l_buffer;
    TRACFCOMP(g_trac_pnor, EXIT_MRK"PnorRP::fixECC");
    return l_err;
}

uint64_t PnorRP::getTocOffset(TOCS i_toc) const
{
    // Can use a ternary operator because there are only 2 TOCs per side
    return (i_toc == TOC_0) ? iv_activeTocOffsets.first :
                              iv_activeTocOffsets.second;
}
