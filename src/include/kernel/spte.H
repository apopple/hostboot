/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/include/kernel/spte.H $                                   */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* COPYRIGHT International Business Machines Corp. 2011,2014              */
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
/** @file spte.H
 *  @brief Defines the structure of the Shadow Page Table Entry.
 */
#ifndef __KERNEL_SPTE_H
#define __KERNEL_SPTE_H

#include <stdint.h>
#include <builtins.h>

/** @class ShadowPTE
 *  @brief Stores information needed in a shadow page table for virtual to
 *         physical address mapping, such as physical page number and
 *         security permissions.
 *
 *  @note This structure only allows addressing of 4GB of physical memory due
 *        to the page number being stored in a 20 bit field.
 *
 *  The data within is stored in a way so that the union value is an address
 *  within the physical page who's number is stored and the low-order bits
 *  are used for storing misc information about the page, such as permissions.
 */
class ShadowPTE
{
    protected:
        union
        {
            uint32_t word;
            struct
            {
                    /** Physical page number. */
                uint32_t page:20;
                    /** Page is present (is PN valid?). */
                uint32_t present:1;
                    /** May the page be read        */
		uint32_t readable:1;
                    /** May the page be written to. */
                uint32_t writable:1;
                    /** May code be executed off page. */
                uint32_t executable:1;

                    /** Should the dirty bit be maintained. */
                uint32_t track_write:1;
                    /** Has page been written to. */
                uint32_t dirty:1;
                    /** Allocate from a zero'd page. */
                uint32_t allocate_from_zero:1;
                    /** LRU value - lower means it was accessed more recently. */
                uint32_t last_access:3;
                    /** Reserved for future use. */
                uint32_t reserved:2;
            } PACKED;
        };

    public:
            /** Initialize PTE */
        ShadowPTE() : word(0) {};
            /** Cast-construct from integer directly to the data union. */
        explicit ShadowPTE(uint32_t i_data) : word(i_data) {};

            /** Get physical page (as address). */
        uint32_t getPageAddr() const { return (page << 12); };
            /** Set physical page (as address). */
        void setPageAddr(uint32_t i_page) { page = (i_page >> 12); };
            /** Get physical page (as page number). */
        uint32_t getPage() const { return page; }

            /** Get present bit. */
        bool isPresent() const { return present; };
            /** Set present bit. */
        void setPresent(bool i_present) { present = i_present; };
            /** Get readable bit. */
        bool isReadable() const { return readable; };
            /** Set readable bit. */
        void setReadable(bool i_read) { readable = i_read; };
            /** Get writable bit. */
        bool isWritable() const { return writable; };
            /** Set writable bit. */
        void setWritable(bool i_write) { writable = i_write; };
            /** Get executable bit. */
        bool isExecutable() const { return executable; };
            /** Set executable bit. */
        void setExecutable(bool i_exec) { executable = i_exec; };

            /** Get write-tracked bit. */
        bool isWriteTracked() const { return track_write; };
            /** Set write-tracked bit. */
        void setWriteTracked(bool i_track) { track_write = i_track; };
            /** Get dirty bit. */
        bool isDirty() const { return dirty; };
            /** Set dirty bit. */
        void setDirty(bool i_dirty) { dirty = i_dirty; };

            /** Get allocate-from-zero bit. */
        bool isAllocateFromZero() const { return allocate_from_zero; };
            /** Set allocate-from-zero bit. */
        void setAllocateFromZero(bool i_zero) { allocate_from_zero = i_zero; };

            /** Get last_acces bits. */
        uint8_t getLRU() const { return last_access; };
            /** Increment the LRU bits. */
        void incLRU() {
	    if( last_access < 0b111 )
	    {
		last_access++;
	    }
	};
            /** Decrement the LRU bits. */
        void decLRU() {
	    if( last_access > 0 )
	    {
		last_access--;
	    }
	};
            /** Zero out the LRU bits. */
        void zeroLRU() {
	    last_access = 0;
	};


};

#endif
