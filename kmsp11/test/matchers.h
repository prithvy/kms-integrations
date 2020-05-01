#ifndef KMSP11_TEST_MATCHERS_H_
#define KMSP11_TEST_MATCHERS_H_

#include <regex>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "gmock/gmock.h"
#include "google/protobuf/util/message_differencer.h"
#include "kmsp11/util/status_or.h"
#include "kmsp11/util/status_utils.h"

namespace kmsp11 {

// A regex matcher with the same signature as ::testing::MatchesRegex, but whose
// implementation is backed by std::regex.
//
// Unfortunately ::testing::MatchesRegex does not operate consistently across
// platforms:
// https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#regular-expression-syntax
MATCHER_P(MatchesStdRegex, pattern,
          absl::StrFormat("%s regex '%s'",
                          (negation ? "doesn't match" : "matches"),
                          testing::PrintToString(pattern))) {
  return std::regex_match(std::string(arg), std::regex(pattern));
}

// Tests that the supplied status has the expected status code.
MATCHER_P(StatusIs, status_code,
          absl::StrFormat("status is %s%s", (negation ? "not " : ""),
                          testing::PrintToString(status_code))) {
  return ToStatus(arg).code() == status_code;
}

// Tests that the supplied status has the expected CK_RV.
MATCHER_P(StatusRvIs, ck_rv,
          absl::StrFormat("status ck_rv is %s%#x", (negation ? "not " : ""),
                          ck_rv)) {
  return GetCkRv(ToStatus(arg)) == ck_rv;
}

// Tests that the supplied status is OK.
MATCHER(IsOk, absl::StrFormat("status is %sOK", negation ? "not " : "")) {
  return ToStatus(arg).ok();
}

// Tests that the supplied protocol buffer message is equal.
MATCHER_P(EqualsProto, proto,
          absl::StrFormat("proto %s", negation ? "does not equal" : "equals")) {
  return google::protobuf::util::MessageDifferencer::Equals(arg, proto);
}

}  // namespace kmsp11

#endif  // KMSP11_TEST_MATCHERS_H_
