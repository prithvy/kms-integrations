// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>

#include "kmsp11/test/test_platform.h"
#include "kmsp11/util/errors.h"

namespace kmsp11 {

void SetEnvVariable(const char* name, const char* value) {
  _putenv_s(name, value);
}

void ClearEnvVariable(const char* name) { _putenv_s(name, ""); }

absl::Status SetMode(const char* filename, int mode) {
  return NewError(absl::StatusCode::kUnimplemented,
                  "SetMode is not implemented on Windows", CKR_GENERAL_ERROR,
                  SOURCE_LOCATION);
}

}  // namespace kmsp11
