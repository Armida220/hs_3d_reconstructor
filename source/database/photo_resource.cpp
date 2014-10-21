﻿#include "database/photo_resource.hpp"

namespace hs
{
namespace recon
{
namespace db
{

Field PhotoResource::fields_[number_of_fields_] =
{
  {"ID", FIELD_VALUE_INTEGER, FIELD_CONSTRAINT_KEY},
  {"PHOTOGROUP_ID", FIELD_VALUE_INTEGER, FIELD_CONSTRAINT_NONE},
  {"PATH", FIELD_VALUE_TEXT, FIELD_CONSTRAINT_NONE},
  {"POS_X", FIELD_VALUE_REAL, FIELD_CONSTRAINT_NONE},
  {"POS_Y", FIELD_VALUE_REAL, FIELD_CONSTRAINT_NONE},
  {"POS_Z", FIELD_VALUE_REAL, FIELD_CONSTRAINT_NONE},
  {"PITCH", FIELD_VALUE_REAL, FIELD_CONSTRAINT_NONE},
  {"ROLL", FIELD_VALUE_REAL, FIELD_CONSTRAINT_NONE},
  {"HEADING", FIELD_VALUE_REAL, FIELD_CONSTRAINT_NONE}
};

const char* PhotoResource::resource_name_ = "PHOTO";

DEFINE_RESOURCE_COMMON_MEMBERS(PhotoResource)

}
}
}
