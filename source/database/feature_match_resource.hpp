﻿#ifndef _HS_3D_RECONSTRUCTOR_DATABASE_FEATURE_MATCH_HPP_
#define _HS_3D_RECONSTRUCTOR_DATABASE_FEATURE_MATCH_HPP_

#include "hs_3d_reconstructor/config/hs_config.hpp"

#include "database/resource_utility.hpp"

namespace hs
{
namespace recon
{
namespace db
{

class HS_EXPORT FeatureMatchResource
{
public:
  enum FieldIdentifier
  {
    FEATURE_MATCH_FIELD_ID = 0,
    FEATURE_MATCH_FIELD_BLOCK_ID,
    FEATURE_MATCH_FIELD_NAME,
    FEATURE_MATCH_FIELD_FLAG,
    NUMBER_OF_FEATURE_MATCH_FIELDS
  };

  enum Flag
  {
    FLAG_NOT_COMPLETED,
    FLAG_COMPLETED
  };
  static const int number_of_fields_ = NUMBER_OF_FEATURE_MATCH_FIELDS;
  DECLARE_RESOURCE_COMMON_MEMBERS(FeatureMatchResource)

};

}
}
}

#endif
