/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/diag/prdf/common/framework/config/iipDomain.h $       */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* COPYRIGHT International Business Machines Corp. 1996,2014              */
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

#ifndef iipDomain_h
#define iipDomain_h


#include <iipconst.h>    // Include file for DOMAIN_ID's
#include <iipchip.h>     // @jl02
#ifndef IIPSDBUG_H
#include <iipsdbug.h>    // Include file for ATTENTION_TYPE
#endif

namespace PRDF
{

// Forward References
struct STEP_CODE_DATA_STRUCT;

/*!
 Domain class provides error analysis of a specific domain of a hardware system

 Usage Examples:
 @code
   // during PrdInitialize()
   Domain * domain = new DerivedDomain(id,...);
   int32_t rc=domain->Initialize(); // Perform domain dependent hardware init

   // During PRD Analyze: called from System_Class
   if (domain->Query())                // Query for domain at attention
   {
   // Analyze the attention
   int32_t rc=domain->Analyze(service_data,System_attention_type);
   }

   DOMAIN_ID id = domain->GetId();        - Get the current domains id.
 @endcode
 */
class Domain
{
public:

  /**
   Contructor
   @param domain_id   Id of this domain. See iipconst.h
   */
  Domain(DOMAIN_ID domain_id);

  /**
   Destructor
   @note Default does nothing - must be virtual for derived classes
   */
  virtual ~Domain(void);

  /**
   Initialize domain specific hardware as needed
   @return MOPS error code or @c prd_return_code_t
   */
  virtual int32_t Initialize(void);

  /**
   Remove domain specific hardware as needed
   @return MOPS error code or @c prd_return_code_t
   */
  virtual void Remove(TARGETING::TargetHandle_t);

  /**
   Query - if any sub components have attention matching attentionType
   @param attentionType  see iipsdbug.h for values
   @return true|false
   @pre this->Initialize() must be called
   */
  virtual bool Query(ATTENTION_TYPE attentionType) = 0;

  /**
   Analzye this domain
   @param attentionType to analyze
   @return Mops return code | @c prd_return_code_t
   @return serviceData
   @pre this->Query() == true
   @post serviceData valid
   */
  virtual int32_t Analyze(STEP_CODE_DATA_STRUCT & serviceData,
                          ATTENTION_TYPE attentionType) = 0;

  /**
   Access the ID of this domain
   @return @c DOMAIN_ID  See iipconst.h
   */
  DOMAIN_ID GetId(void) const { return(dom_id); }

protected:

  /**
   Prioritize the components of this domain for Analysis
   @param ATTENTION_TYPE [MACHINE_CHECK, RECOVERABLE, SPECIAL]
   @post Domain prepared for Analysis
   @note Default is do nothing
   */
  virtual void Order(ATTENTION_TYPE attentionType) = 0;

private:

  DOMAIN_ID     dom_id;

};

} // end namespace PRDF

#endif
