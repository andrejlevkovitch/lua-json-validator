// lua_json_validator.cpp

#include <lua.hpp>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using json = nlohmann::json;
using json_validator = nlohmann::json_schema::json_validator;

const json draft7 = json::parse(R"(
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "$id": "http://json-schema.org/draft-07/schema#",
    "title": "Core schema meta-schema",
    "definitions": {
        "schemaArray": {
            "type": "array",
            "minItems": 1,
            "items": { "$ref": "#" }
        },
        "nonNegativeInteger": {
            "type": "integer",
            "minimum": 0
        },
        "nonNegativeIntegerDefault0": {
            "allOf": [
                { "$ref": "#/definitions/nonNegativeInteger" },
                { "default": 0 }
            ]
        },
        "simpleTypes": {
            "enum": [
                "array",
                "boolean",
                "integer",
                "null",
                "number",
                "object",
                "string"
            ]
        },
        "stringArray": {
            "type": "array",
            "items": { "type": "string" },
            "uniqueItems": true,
            "default": []
        }
    },
    "type": ["object", "boolean"],
    "properties": {
        "$id": {
            "type": "string"
        },
        "$schema": {
            "type": "string"
        },
        "$ref": {
            "type": "string"
        },
        "$comment": {
            "type": "string"
        },
        "title": {
            "type": "string"
        },
        "description": {
            "type": "string"
        },
        "default": true,
        "readOnly": {
            "type": "boolean",
            "default": false
        },
        "writeOnly": {
            "type": "boolean",
            "default": false
        },
        "examples": {
            "type": "array",
            "items": true
        },
        "multipleOf": {
            "type": "number",
            "exclusiveMinimum": 0
        },
        "maximum": {
            "type": "number"
        },
        "exclusiveMaximum": {
            "type": "number"
        },
        "minimum": {
            "type": "number"
        },
        "exclusiveMinimum": {
            "type": "number"
        },
        "maxLength": { "$ref": "#/definitions/nonNegativeInteger" },
        "minLength": { "$ref": "#/definitions/nonNegativeIntegerDefault0" },
        "pattern": {
            "type": "string"
        },
        "additionalItems": { "$ref": "#" },
        "items": {
            "anyOf": [
                { "$ref": "#" },
                { "$ref": "#/definitions/schemaArray" }
            ],
            "default": true
        },
        "maxItems": { "$ref": "#/definitions/nonNegativeInteger" },
        "minItems": { "$ref": "#/definitions/nonNegativeIntegerDefault0" },
        "uniqueItems": {
            "type": "boolean",
            "default": false
        },
        "contains": { "$ref": "#" },
        "maxProperties": { "$ref": "#/definitions/nonNegativeInteger" },
        "minProperties": { "$ref": "#/definitions/nonNegativeIntegerDefault0" },
        "required": { "$ref": "#/definitions/stringArray" },
        "additionalProperties": { "$ref": "#" },
        "definitions": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "properties": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "patternProperties": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "dependencies": {
            "type": "object",
            "additionalProperties": {
                "anyOf": [
                    { "$ref": "#" },
                    { "$ref": "#/definitions/stringArray" }
                ]
            }
        },
        "propertyNames": { "$ref": "#" },
        "const": true,
        "enum": {
            "type": "array",
            "items": true,
            "minItems": 1,
            "uniqueItems": true
        },
        "type": {
            "anyOf": [
                { "$ref": "#/definitions/simpleTypes" },
                {
                    "type": "array",
                    "items": { "$ref": "#/definitions/simpleTypes" },
                    "minItems": 1,
                    "uniqueItems": true
                }
            ]
        },
        "format": { "type": "string" },
        "contentMediaType": { "type": "string" },
        "contentEncoding": { "type": "string" },
        "if": { "$ref": "#" },
        "then": { "$ref": "#" },
        "else": { "$ref": "#" },
        "allOf": { "$ref": "#/definitions/schemaArray" },
        "anyOf": { "$ref": "#/definitions/schemaArray" },
        "oneOf": { "$ref": "#/definitions/schemaArray" },
        "not": { "$ref": "#" }
    },
    "default": true,
    "additionalProperties": false
}
)");

class ValidatorErrorHandler
    : public nlohmann::json_schema::basic_error_handler {
  void error(const nlohmann::json::json_pointer &ptr,
             [[maybe_unused]] const nlohmann::json &instance,
             const std::string &message) override {
    ss << "Error: '" << ptr << "' : " << message << std::endl;

    nlohmann::json_schema::basic_error_handler::error(ptr, instance, message);
  }

public:
  std::stringstream ss;
};

extern "C" {
/**\brief has two params: schema (as string) and json (as string). In case of
 * error or validation failed return nil and error string, in case of success
 * return validated body (this can some new data, for example if set some
 * default params)
 */
static int lua_json_schema_validate(lua_State *state) {
  std::string schemaStr = luaL_checkstring(state, 1);
  std::string jsonStr = luaL_checkstring(state, 2);

  json schema = json::parse(schemaStr, nullptr, false);
  if (schema.is_discarded()) {
    lua_pushnil(state);
    lua_pushstring(state, "invalid schema");
    return 2;
  }

  json forValidation = json::parse(jsonStr, nullptr, false);
  if (schema.is_discarded()) {
    lua_pushnil(state);
    lua_pushstring(state, "invalid json");
    return 2;
  }

  json_validator validator;
  try {
    validator.set_root_schema(schema);
  } catch (std::exception &e) {
    lua_pushnil(state);
    lua_pushstring(state, e.what());
    return 2;
  }

  ValidatorErrorHandler error;
  json patch = validator.validate(forValidation, error);
  if (error) {
    lua_pushnil(state);
    lua_pushstring(state, error.ss.str().c_str());
    return 2;
  }

  if (patch.empty()) { // in this case just push original string without any
                       // changes and without copy
    lua_pushvalue(state, 2);
  } else { // in this case aply some patch and return patched json string
    json outputJson = forValidation.patch(patch);
    std::string outJsonStr = outputJson.dump();
    lua_pushlstring(state, outJsonStr.c_str(), outJsonStr.size());
  }

  return 1;
}

/**\brief has one param: json schema (as string). Return true in case of
 * success, or nil and error string otherwise
 */
static int lua_json_schema_check(lua_State *state) {
  std::string schemaStr = luaL_checkstring(state, 1);

  json schema = json::parse(schemaStr, nullptr, false);
  if (schema.is_discarded()) {
    lua_pushnil(state);
    lua_pushstring(state, "invalid json");
    return 2;
  }

  json_validator validator;
  validator.set_root_schema(draft7);

  ValidatorErrorHandler error;
  validator.validate(schema, error);
  if (error) {
    lua_pushnil(state);
    lua_pushstring(state, error.ss.str().c_str());
    return 2;
  }

  lua_pushboolean(state, true);
  return 1;
}

static luaL_Reg json_validator_table[]{{"check_schema", lua_json_schema_check},
                                       {"validate", lua_json_schema_validate},
                                       {nullptr, nullptr}};

int luaopen_json_validator(lua_State *state) {
  luaL_register(state, "json_validator", json_validator_table);
  return 1;
}
}
