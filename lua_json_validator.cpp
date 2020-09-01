// lua_json_validator.cpp

#include <fstream>
#include <lua.hpp>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#define VALIDATOR_INSTANCE_METATABLE "json_validator_instance"

using json           = nlohmann::json;
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
  void error(const nlohmann::json::json_pointer &   ptr,
             [[maybe_unused]] const nlohmann::json &instance,
             const std::string &                    message) override {
    std::stringstream ss;
    ss << "json schema error: '" << ptr << "' : " << message;
    throw std::invalid_argument{ss.str()};
  }
};

extern "C" {
/**\param 1 schema as string
 * \param 2 json for validation as string
 * \return validated json as string or nil and error string
 */
static int lua_json_schema_validate(lua_State *state) {
  std::string schemaStr = luaL_checkstring(state, 1);
  std::string jsonStr   = luaL_checkstring(state, 2);

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
  json                  patch;
  try {
    patch = validator.validate(forValidation, error);
  } catch (std::exception &e) {
    lua_pushnil(state);
    lua_pushstring(state, e.what());
    return 2;
  }

  if (patch.empty()) { // in this case just push original string without any
                       // changes and without copy
    lua_pushvalue(state, 2);
  } else { // in this case aply some patch and return patched json string
    json        outputJson = forValidation.patch(patch);
    std::string outJsonStr = outputJson.dump();
    lua_pushlstring(state, outJsonStr.c_str(), outJsonStr.size());
  }

  return 1;
}

/**\brief validate json schema
 * \param 1 json schema as string
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
  try {
    validator.validate(schema, error);
  } catch (std::exception &e) {
    lua_pushnil(state);
    lua_pushstring(state, e.what());
    return 2;
  }

  lua_pushboolean(state, true);
  return 1;
}

/**\brief create new instance of validator with some schema
 * \param 1 json schema as string
 * \param 2 directory with additional json schema files, not required
 */
static int lua_json_validator_new(lua_State *state) {
  std::string schemaStr = luaL_checkstring(state, 1);
  std::string schemaDir;

  if (lua_gettop(state) >= 2) {
    schemaDir = luaL_checkstring(state, 2);
  }

  json schema = json::parse(schemaStr, nullptr, false);
  if (schema.is_discarded()) {
    lua_pushnil(state);
    lua_pushstring(state, "invalid json");
    return 2;
  }

  auto additionalSchemaLoader = [schemaDir](const nlohmann::json_uri &id,
                                            json &                    value) {
    std::string   filename = schemaDir + '/' + id.path();
    std::ifstream fin{filename, std::ios::in};
    if (fin.is_open() == false) {
      throw std::invalid_argument{"can't open file: " + filename};
    }
    std::string buf;
    std::copy(std::istreambuf_iterator<char>{fin},
              std::istreambuf_iterator<char>{},
              std::back_inserter(buf));

    value = json::parse(buf, nullptr, false);
    if (value.is_discarded()) {
      throw std::invalid_argument{"invalid json in file: " + filename};
    }
  };

  // end push validator instance as table
  void *          addr      = lua_newuserdata(state, sizeof(json_validator));
  json_validator *validator = new (addr) json_validator{additionalSchemaLoader};

  try {
    validator->set_root_schema(schema);
  } catch (std::exception &e) {
    validator->~json_validator();

    lua_pop(state, 1);

    lua_pushnil(state);
    lua_pushstring(state, e.what());
    return 2;
  }

  luaL_getmetatable(state, VALIDATOR_INSTANCE_METATABLE);
  lua_setmetatable(state, -2);

  return 1;
}

static int lua_json_validator_destroy(lua_State *state) {
  void *addr = luaL_checkudata(state, 1, VALIDATOR_INSTANCE_METATABLE);
  json_validator *validator = reinterpret_cast<json_validator *>(addr);
  validator->~json_validator();
  return 0;
}

/**\brief validate json
 * \param 1 self
 * \param 2 json as strign
 * \return input json as string in case of success, othewise nil and error
 * string
 */
static int lua_json_validator_validate(lua_State *state) {
  void *      addr = luaL_checkudata(state, 1, VALIDATOR_INSTANCE_METATABLE);
  std::string forCheckStr = luaL_checkstring(state, 2);

  json_validator *validator    = reinterpret_cast<json_validator *>(addr);
  json            forCheckJson = json::parse(forCheckStr, nullptr, false);

  ValidatorErrorHandler error;
  json                  patch;
  try {
    patch = validator->validate(forCheckJson, error);
  } catch (std::exception &e) {
    lua_pushnil(state);
    lua_pushstring(state, e.what());
    return 2;
  }

  if (patch.empty()) { // in this case just push original string without any
                       // changes and without copy
    lua_pushvalue(state, 2);
  } else { // in this case aply some patch and return patched json string
    json        outputJson = forCheckJson.patch(patch);
    std::string outJsonStr = outputJson.dump();
    lua_pushlstring(state, outJsonStr.c_str(), outJsonStr.size());
  }

  return 1;
}

static luaL_Reg json_validator_table[]{
    {"check_schema", lua_json_schema_check},
    {"validate", lua_json_schema_validate},
    {"new", lua_json_validator_new},
    {nullptr, nullptr},
};

static luaL_Reg json_validator_metatable[]{
    {"__gc", lua_json_validator_destroy},
    {"validate", lua_json_validator_validate},
    {nullptr, nullptr},
};

int luaopen_json_validator(lua_State *state) {
  // at first register metatable for validator
  luaL_newmetatable(state, VALIDATOR_INSTANCE_METATABLE);
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  for (int i = 0; json_validator_metatable[i].name != nullptr; ++i) {
    lua_pushcfunction(state, json_validator_metatable[i].func);
    lua_setfield(state, -2, json_validator_metatable[i].name);
  }
  lua_pop(state, 1);

  // and register module
  luaL_register(state, "json_validator", json_validator_table);
  return 1;
}
}
