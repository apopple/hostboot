/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/trace/service.C $                                     */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2012,2015                        */
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
#include "service.H"
#include "buffer.H"
#include "entry.H"
#include "compdesc.H"
#include "daemonif.H"
#include "debug.H"

#include <sys/time.h>
#include <sys/task.h>
#include <util/align.H>
#include <string.h>
#include <util/singleton.H>
#include <assert.h>
#include <time.h>
#include <config.h>
#include <console/consoleif.H>
#include <stdio.h>
#include <ctype.h>

namespace TRACE
{
    Service::Service()
    {
        iv_daemon = new DaemonIf();
        iv_buffers[BUFFER_SLOW] = new Buffer(iv_daemon, Buffer::UNLIMITED);
        iv_buffers[BUFFER_FAST] = new Buffer(iv_daemon);
        iv_compList = &(Singleton<ComponentList>::instance());
    }

    Service::~Service()
    {
        // No need to destruct the service.
        assert(0);
    }

    void Service::writeEntry(ComponentDesc* i_td,
                             trace_hash_val i_hash,
                             const char * i_fmt,
                             uint32_t i_line,
                             uint32_t i_type,
                             va_list i_args)
    {
        // Skip writing trace if debug is disabled.
        if (unlikely(i_type == TRACE_DEBUG))
        {
            if ((!i_td->iv_debugEnabled) &&
                (!g_debugSettings.globalDebugEnable))
            {
                return;
            }
        }

        #ifdef CONFIG_CONSOLE_OUTPUT_TRACE
        {
            va_list args;
            va_copy(args, i_args);
            CONSOLE::vdisplayf(i_td->iv_compName, i_fmt, i_args);
            va_end(args);
        }
        #endif

        do
        {
            // Get the right buffer for this component.
            Buffer* l_buffer = iv_buffers[i_td->iv_bufferType];

            // Copy the current time.
            trace_entry_stamp_t l_time;
            _createTimeStamp(&l_time);

            // Sizes for trace entry.
            uint32_t l_size = 0;
            uint32_t l_num_args = 0;
            uint32_t l_num_words = 0;

            // Maps of arguments to special types.
            uint64_t l_str_map = 0, l_char_map = 0, l_double_map = 0;

            va_list l_args;
            va_copy(l_args, i_args);

            // Parse the fmt list to determine the types/sizes of each
            // argument.
            while(NULL != (i_fmt = strchr(i_fmt, '%')))
            {
                i_fmt++;

                switch (*i_fmt)
                {
                    case '%':
                        break;

                    case 's': // string.
                        l_str_map |= (1 << l_num_args);
                        l_num_args++;
                        l_num_words++;
                        l_size += ALIGN_4(strlen(va_arg(l_args, char*)) + 1);
                        break;

                    case 'c': // character.
                        l_char_map |= (1 << l_num_args);
                        l_num_args++;
                        l_num_words++;
                        va_arg(l_args, uint32_t);
                        l_size += sizeof(uint32_t);
                        break;

                    case 'e': // doubles.
                    case 'f':
                    case 'g':
                        l_double_map |= (1 << l_num_args);
                        l_num_args++;
                        l_num_words += 2;
                        va_arg(l_args, double);
                        l_size += sizeof(double);
                        break;

                    default: // uint64_t-sized argument.
                        l_num_args++;
                        l_num_words += 2;
                        va_arg(l_args, uint64_t);
                        l_size += sizeof(uint64_t);
                        break;
                }
                i_fmt++;
            }

            va_end(l_args);

            // Ensure we don't have too many arguments.
            if (l_num_args > TRAC_MAX_ARGS)
            {
                // Simply reducing the number of arguments and continuing
                // causes FSP trace to crash.  See defect 864438.
                //l_num_args = TRAC_MAX_ARGS;
                break;
            }

            // Claim an entry from the buffer.
            uint32_t l_realSize = ALIGN_4(sizeof(trace_bin_entry_t) + l_size);
            Entry* l_entry = l_buffer->claimEntry(i_td, l_realSize);
            if (NULL == l_entry)
            {
                break;
            }

            // Copy contents into the 'fsp-trace'-style structure.
            trace_bin_entry_t* l_binEntry =
                reinterpret_cast<trace_bin_entry_t*>(&l_entry->data[0]);

            l_binEntry->stamp = l_time;
            l_binEntry->head.length = l_size;
            l_binEntry->head.tag = TRACE_COMP_TRACE;
            l_binEntry->head.hash = i_hash;
            l_binEntry->head.line = i_line;


            // Copy arguments into the 'fsp-trace' entry's data section.
            char* l_dataOut = reinterpret_cast<char*>(&l_binEntry->data[0]);
            for (size_t i = 0; i < l_num_args; i++)
            {
                // String.
                if (l_str_map & (1 << i))
                {
                    char* str = va_arg(i_args, char*);
                    uint32_t strSize = strlen(str);

                    memcpy(l_dataOut, str, strSize+1);
                    l_dataOut += ALIGN_4(strSize + 1);
                }
                // Single character.
                else if (l_char_map & (1 << i))
                {
                    *(reinterpret_cast<uint32_t*>(l_dataOut)) =
                        va_arg(i_args, uint32_t);
                    l_dataOut += sizeof(uint32_t);
                }
                // Doubles.
                else if (l_double_map & (1 << i))
                {
                    *(reinterpret_cast<double*>(l_dataOut)) =
                        va_arg(i_args, double);
                    l_dataOut += sizeof(double);
                }
                // All others (as uint64_t's).
                else
                {
                    *(reinterpret_cast<uint64_t*>(l_dataOut)) =
                        va_arg(i_args, uint64_t);
                    l_dataOut += sizeof(uint64_t);
                }
            }

            // "Commit" entry to buffer.
            l_buffer->commitEntry(l_entry);

        } while(0);
    }

    void Service::writeBinEntry(ComponentDesc* i_td,
                                trace_hash_val i_hash,
                                const char * i_fmt,
                                uint32_t i_line,
                                const void* i_ptr,
                                uint32_t i_size,
                                uint32_t i_type)
    {
        // Skip writing trace if debug is disabled.
        if (unlikely(i_type == TRACE_DEBUG))
        {
            if ((!i_td->iv_debugEnabled) &&
                (!g_debugSettings.globalDebugEnable))
            {
                return;
            }
        }

        #ifdef CONFIG_CONSOLE_OUTPUT_TRACE
        {
        // Output binary data.
        // Format is:
        // ~[0x0000] 01234567 01234567 01234567 01234567    *012345689abcdef*
            static size_t BINARY_FORMAT_LENGTH = 69;

            size_t pos = 0;
            char* output = strdup(i_fmt);
            size_t output_size = strlen(output)+1;
            while(pos < i_size)
            {
                char bin_output[BINARY_FORMAT_LENGTH];

                // Create length header.
                size_t bin_pos = sprintf(bin_output,
                                         "~[0x%04hx]", (uint16_t) pos);

                // Create hex representation.
                for (int i =0 ; i < 16; ++i)
                {
                    if ((i % 4) == 0)
                    {
                        bin_output[bin_pos++] = ' ';
                    }
                    if ((pos + i) < i_size)
                    {
                        sprintf(&bin_output[bin_pos],
                                "%02x", ((const char*)i_ptr)[pos+i]);
                        bin_pos += 2;
                    }
                    else
                    {
                        bin_output[bin_pos++] = ' ';
                        bin_output[bin_pos++] = ' ';
                    }
                }

                for (int i = 0; i < 4; ++i)
                {
                    bin_output[bin_pos++] = ' ';
                }
                bin_output[bin_pos++] = '*';

                // Create ascii representation.
                for(int i = 0; i< 16; ++i)
                {
                    char ch = ' ';
                    if ((pos + i) < i_size)
                    {
                        ch = ((const char*) i_ptr)[pos+i];
                        if (!isprint(ch))
                        {
                            ch = '.';
                        }
                    }
                    bin_output[bin_pos++] = ch;
                }
                bin_output[bin_pos++] = '*';
                bin_output[bin_pos++] = '\0';

                // Append to output string.
                output = static_cast<char*>(
                    realloc(output, output_size + BINARY_FORMAT_LENGTH));
                strcat(output, "\n");
                strcat(output, bin_output);
                output_size += BINARY_FORMAT_LENGTH;

                pos += 16;
            }

            // Output full binary dump.
            CONSOLE::displayf(i_td->iv_compName, output);
            free(output);

        }
        #endif


        do
        {
            // Get the right buffer for this component.
            Buffer* l_buffer = iv_buffers[i_td->iv_bufferType];

            // Copy the current time.
            trace_entry_stamp_t l_time;
            _createTimeStamp(&l_time);

            // Claim an entry from the buffer.
            uint32_t l_realSize = ALIGN_4(sizeof(trace_bin_entry_t) + i_size);
            Entry* l_entry = l_buffer->claimEntry(i_td, l_realSize);
            if (NULL == l_entry)
            {
                break;
            }

            // Copy contents into the 'fsp-trace'-style structure.
            trace_bin_entry_t* l_binEntry =
                reinterpret_cast<trace_bin_entry_t*>(&l_entry->data[0]);

            l_binEntry->stamp = l_time;
            l_binEntry->head.length = i_size;
            l_binEntry->head.tag = TRACE_FIELDBIN;
            l_binEntry->head.hash = i_hash;
            l_binEntry->head.line = i_line;

            // Copy bin-data into the 'fsp-trace' entry's data section.
            memcpy(&l_binEntry->data[0], i_ptr, i_size);

            // "Commit" entry to buffer.
            l_buffer->commitEntry(l_entry);

        } while(0);
    }

    size_t Service::getBuffer(ComponentDesc* i_comp,
                              void * o_data,
                              size_t i_size)
    {
        return
            iv_buffers[i_comp->iv_bufferType]->getTrace(i_comp,o_data,i_size);
    }

    void Service::flushBuffers()
    {
        iv_daemon->signal(true);
    }

    Service* Service::getGlobalInstance()
    {
        return &(Singleton<Service>::instance());
    }

    void Service::_createTimeStamp(trace_entry_stamp_t *o_entry)
    {
        timespec_t curTime;

        clock_gettime(CLOCK_MONOTONIC, &curTime);

        o_entry->tbh = curTime.tv_sec;
        o_entry->tbl = curTime.tv_nsec;
        o_entry->tid = task_gettid();
    }


}
