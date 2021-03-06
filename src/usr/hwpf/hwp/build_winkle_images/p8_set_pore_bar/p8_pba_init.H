/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/hwpf/hwp/build_winkle_images/p8_set_pore_bar/p8_pba_init.H $ */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2012,2014                        */
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

// $Id: p8_pba_init.H,v 1.8 2014/03/17 23:16:35 stillgs Exp $

//------------------------------------------------------------------------------
// *! (C) Copyright International Business Machines Corp. 2011
// *! All Rights Reserved -- Property of IBM
// *! ***  ***
//------------------------------------------------------------------------------
// *! OWNER NAME: Klaus P. Gungl         Email: kgungl@de.ibm.com
// *!
// *! General Description:
// *!
// *!  include file for pba_init with constants, definitions, prototypes
// *!
//------------------------------------------------------------------------------
//

#ifndef _P8_PBAINIT_H_
#define _P8_PBAINIT_H_

#include "p8_scom_addresses.H"

typedef fapi::ReturnCode (*p8_pba_init_FP_t) (const fapi::Target& , uint64_t );

// constant definitions for valid command scope. LIMIT is used by setup routine
// for plausibility checking.

#define PBA_CMD_SCOPE_NODAL    0x00
#define PBA_CMD_SCOPE_GROUP    0x01
#define PBA_CMD_SCOPE_SYSTEM   0x02
#define PBA_CMD_SCOPE_RGP      0x03
#define PBA_CMD_SCOPE_FOREIGN0 0x04
#define PBA_CMD_SCOPE_FOREIGN1 0x05
#define PBA_CMD_SCOPE_LIMIT    0x06

enum cmd_scope_t
{
    CMD_SCOPE_NODAL,
    CMD_SCOPE_GROUP,
    CMD_SCOPE_SYSTEM,
    CMD_SCOPE_RGP,
    CMD_SCOPE_FOREIGN0,
    CMD_SCOPE_FOREIGN1
};
// enum cmd_scope_type {NODAL, GROUP, SYSTEM, RGP, FOREIGN0, FOREIGN1 };


// addresses of PBA and PBABAR, actually a duplicate of definitions in
// "p8_scom_addresses.H" but here an array to be indexed.
const uint64_t PBA_BARs[4] =
{
    PBA_BAR0_0x02013F00,
    PBA_BAR1_0x02013F01,
    PBA_BAR2_0x02013F02,
    PBA_BAR3_0x02013F03
};

const uint64_t PBA_BARMSKs[4] =
{
    PBA_BARMSK0_0x02013F04,
    PBA_BARMSK1_0x02013F05,
    PBA_BARMSK2_0x02013F06,
    PBA_BARMSK3_0x02013F07
};

const uint64_t PBA_SLVCTLs[4] =
{
    PBA_SLVCTL0_0x00064004,
    PBA_SLVCTL1_0x00064005,
    PBA_SLVCTL2_0x00064006,
    PBA_SLVCTL3_0x00064007
};

const uint64_t PBA_SLVRESETs[4] =
{
    0x8000000000000000ull,
    0xA000000000000000ull,
    0xC000000000000000ull,
    0xE000000000000000ull
};

// Maximum number of Polls for PBA slave reset
#define MAX_PBA_RESET_POLLS     16
#define PBA_RESET_POLL_DELAY    1       // in microseconds

// Maximum number of Polls for PBA Block Copy Stopping - 500ms timeout
#define MAX_PBA_BC_STOP_POLLS     256
#define PBA_BC_STOP_POLL_DELAY    10    // in microseconds

// bar mask is valid for bits 23 to 43, in a 64bit value this is
//           1         2         3         4         5         6
// 0123456789012345678901234567890123456789012345678901234567890123
// 0000000000000000000000011111111111111111111100000000000000000000
//    0   0   0   0   0   1   F   F   F   F   F   0   0   0   0   0
// 0000000000000011111111111111111111111111111100000000000000000000
//    0   0   0   3   F   F   F   F   F   F   F   0   0   0   0   0
//                       0123456701234567
#define BAR_MASK_LIMIT 0x000001FFFFF00000ull
#define BAR_ADDR_LIMIT 0x0003FFFFFFF00000ull

// structure of values for cmd_scope, pba and pbabar initialization
typedef struct {
  cmd_scope_t cmd_scope;
  uint64_t addr;
  uint64_t size;
} struct_pba_bar_msk_scope_init_type;

typedef struct {
  struct_pba_bar_msk_scope_init_type regs0;
  struct_pba_bar_msk_scope_init_type regs1;
  struct_pba_bar_msk_scope_init_type regs2;
  struct_pba_bar_msk_scope_init_type regs3;
} pba_init_type;

typedef struct {
  unsigned long reserved_3:20;
  unsigned long addr:30;
  unsigned short reserved_2:10;
  char reserved_1:1;
  uint8_t cmd_scope:3;
} struct_pba_bar_fields_type;

typedef struct {
  unsigned long reserved_1:23;
  unsigned long mask:21;
  unsigned long reserved_2:20;
} struct_pba_barmsk_fields_type;

typedef union {
  struct_pba_bar_fields_type fields;
  uint64_t value;
} bar_reg_type;

typedef union {
  struct_pba_barmsk_fields_type fields ;
  uint64_t value;
} barmsk_reg_type;

typedef struct {
  bar_reg_type     bar_reg;
  barmsk_reg_type  barmsk_reg;
} struct_pba_bar_init_type;


typedef struct {
  struct struct_pba_bar{
    char cmd_scope:3;
    char reserved_1:1;
    unsigned short reserved_2:10;
    unsigned long addr:30;
    unsigned long reserved_3:20;
  };
  struct struct_pba_barmsk{
    unsigned long reserved_1:23;
    unsigned long mask:21;
    unsigned long reserved_2:20;
  };
} struct_pba_bar_init_type_1;

typedef struct {
  struct_pba_bar_init_type pba_bar0_init;
  struct_pba_bar_init_type pba_bar1_init;
  struct_pba_bar_init_type pba_bar2_init;
  struct_pba_bar_init_type pba_bar3_init;
} struct_all_pba_bar_init_type;


typedef uint64_t pba_slvctl_type;


typedef union pbaxcfg_typ{
   uint64_t value;
   struct {
     unsigned long  reserved_0 :20;
     unsigned char  ATTR_PM_PBAX_RCV_RESERV_TIMEOUT :5 ;
     unsigned long  reserved_1 :2;
     unsigned char  ATTR_PM_PBAX_SND_RETRY_COUNT_OVERCOMMIT_ENABLE :1 ;
     unsigned short ATTR_PM_PBAX_SND_RETRY_THRESHOLD :8 ;
     unsigned short ATTR_PM_PBAX_SND_RESERV_TIMEOUT  :5 ;
     unsigned long  reserved_2 :23 ;
   } fields;
} pbaxcfg_t;

// BCDE and BCUE Status registers bits
#define PBA_BC_STAT_RUNNING  0
#define PBA_BC_STAT_STOPPED 29
#define PBA_BC_STAT_ERROR   30
#define PBA_BC_STAT_DONE    31


// ----------------------------------------------------------------------
// Function prototypes
// ----------------------------------------------------------------------

extern "C"
{

fapi::ReturnCode
p8_pba_init (const fapi::Target& i_target,
                     uint64_t mode
              );


} // extern "C"



#endif // _P8_PBAINITQ_H_

