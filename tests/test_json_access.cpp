#include <glaze/json/read.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "common/json/json_access.h"

namespace {

void ExpectTrue(bool condition, const std::string& msg = "") {
  if (!condition) {
    std::cerr << "ExpectTrue failed! " << msg << "\n";
    std::abort();
  }
}

void ExpectEq(const std::string& actual, const std::string& expected, const std::string& msg = "") {
  if (actual != expected) {
    std::cerr << "ExpectEq failed: expected '" << expected << "', got '" << actual << "' (" << msg
              << ")\n";
    std::abort();
  }
}

// FromStrings must build a FLAT JSON array of strings. The glaze json_t
// initializer_list constructor wraps `{value}` into an outer array, which used
// to corrupt every config field written through this helper as `[[...]]`.
void TestFromStringsProducesFlatArray() {
  const glz::json_t value = vinput::json::FromStrings(std::vector<std::string>{"alpha", "beta"});

  const auto* array = value.get_if<glz::json_t::array_t>();
  ExpectTrue(array != nullptr, "FromStrings must yield a json array");
  ExpectTrue(array->size() == 2, "FromStrings array must hold one element per input string");
  for (const auto& item : *array) {
    ExpectTrue(item.get_if<std::string>() != nullptr,
               "FromStrings elements must be strings, not nested arrays");
  }

  ExpectEq(vinput::json::ToText(value), R"(["alpha","beta"])",
           "FromStrings serialization must be a flat string array");
}

// FromStringMap must build a FLAT JSON object. The same initializer_list
// pitfall used to wrap the object into an array: `[{...}]`.
void TestFromStringMapProducesFlatObject() {
  const std::map<std::string, std::string> values{{"key_a", "1"}, {"key_b", "2"}};
  const glz::json_t value = vinput::json::FromStringMap(values);

  const auto* object = value.get_if<glz::json_t::object_t>();
  ExpectTrue(object != nullptr, "FromStringMap must yield a json object");
  ExpectTrue(object->size() == 2, "FromStringMap object must hold one member per input pair");

  ExpectEq(vinput::json::ToText(value), R"({"key_a":"1","key_b":"2"})",
           "FromStringMap serialization must be a flat object");
}

// Round trip: whatever the builders write must survive serialization and be
// read back losslessly by the reader helpers.
void TestRoundTrip() {
  const std::vector<std::string> strings{"alpha", "beta"};
  const glz::json_t array_value = vinput::json::FromStrings(strings);
  const std::string array_text = vinput::json::ToText(array_value);

  glz::json_t parsed_array;
  ExpectTrue(!bool(glz::read_json(parsed_array, array_text)), "array round trip must parse");
  ExpectTrue(vinput::json::Strings(parsed_array) == strings,
             "Strings(FromStrings(x)) must restore the original strings");

  const std::map<std::string, std::string> mapping{{"key_a", "1"}, {"key_b", "2"}};
  const glz::json_t object_value = vinput::json::FromStringMap(mapping);
  const std::string object_text = vinput::json::ToText(object_value);

  glz::json_t parsed_object;
  ExpectTrue(!bool(glz::read_json(parsed_object, object_text)), "object round trip must parse");
  ExpectTrue(vinput::json::StringMap(parsed_object) == mapping,
             "StringMap(FromStringMap(x)) must restore the original map");
}

// Strings()/StringMap() keep skipping foreign element kinds, but plain string
// arrays and string maps must keep working.
void TestReaderHelpersOnPlainInputs() {
  glz::json_t array_value;
  ExpectTrue(!bool(glz::read_json(array_value, R"(["a","b"])")), "fixture must parse");
  ExpectTrue(vinput::json::Strings(array_value) == std::vector<std::string>{"a", "b"});

  glz::json_t object_value;
  ExpectTrue(!bool(glz::read_json(object_value, R"({"k":"v"})")), "fixture must parse");
  const std::map<std::string, std::string> expected{{"k", "v"}};
  ExpectTrue(vinput::json::StringMap(object_value) == expected);
}

} // namespace

int main() {
  TestFromStringsProducesFlatArray();
  TestFromStringMapProducesFlatObject();
  TestRoundTrip();
  TestReaderHelpersOnPlainInputs();
  std::cout << "All test_json_access tests passed!\n";
  return 0;
}
