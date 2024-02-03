//
// Created by Alex2772 on 11/3/2021.
//

#pragma once

#include "converter.hpp"
#include "exception.hpp"
#include "lua.h"
#include "ref.hpp"
#include "util.hpp"
#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace clg {
    using table_array = std::vector<ref>;

    class table: public std::vector<std::pair<std::string, clg::ref>> {
    public:
        using std::vector<std::pair<std::string, clg::ref>>::vector;

        [[nodiscard]]
        table_array toArray() const {
            table_array result;
            result.reserve(size());
            for (const auto& v : *this) {
                try {
                    size_t index = std::stoul(v.first) - 1;
                    if (result.size() <= index) {
                        result.resize(index + 1);
                    }
                    result[index] = v.second;
                } catch (...) {
                    throw clg_exception("could not convert map to array: " + v.first + " is not an integer");
                }
            }
            return result;
        }

        clg::ref& operator[](std::string_view key) {
            auto i = std::find_if(this->begin(), this->end(), [&](const auto& p) {
                return p.first == key;
            });
            if (i == this->end()) {
                emplace_back(std::string(key), clg::ref{});
                return back().second;
            }
            return i->second;
        }

        const clg::ref& operator[](std::string_view key) const {
            auto i = std::find_if(this->begin(), this->end(), [&](const auto& p) {
                return p.first == key;
            });
            if (i == this->end()) {
                throw clg_exception(std::string("no such key: ") + key.data());
            }
            return i->second;
        }
    };

    template<>
    struct converter<clg::table> {
        static clg::converter_result<clg::table> from_lua(lua_State* l, int n) {
            clg::stack_integrity_check c(l);
            if (!lua_istable(l, n)) {
                return converter_error{"not a table"};
            }

            clg::table result;
            if (n < 0) {
                n = lua_gettop(l) + n + 1;
            }
            lua_pushnil(l);
            while (lua_next(l, n) != 0)
            {
                // While traversing a table, do not call lua_tolstring directly on a key, unless you know that the key
                // is actually a string. Recall that lua_tolstring may change the value at the given index; this
                // confuses the next call to lua_next. (Lua manual, lua_next)
                //
                // This is why we should copy the key value.
                lua_pushvalue(l, -2);

                auto key = clg::get_from_lua<std::string>(l, -1);
                auto value = clg::get_from_lua<clg::ref>(l, -2);
                lua_pop(l, 2);

                result[std::move(key)] = std::move(value);
            }
            return result;
        }

        static int to_lua(lua_State* l, const clg::table& t) {
            lua_newtable(l);

            clg::stack_integrity_check check(l);

            for (const auto&[k, v] : t) {
                clg::push_to_lua(l, k);
                clg::push_to_lua(l, v);
                lua_settable(l, -3);
            }

            return 1;
        }
    };

    template<>
    struct converter<table_array> {
        static clg::converter_result<clg::table_array> from_lua(lua_State* l, int n) {
            clg::stack_integrity_check c(l);
            if (!lua_istable(l, n)) {
                return converter_error{"not a table"};
            }

            clg::table_array result;
            if (n < 0) {
                n = lua_gettop(l) + n + 1;
            }
            lua_pushnil(l);
            while (lua_next(l, n) != 0)
            {
                auto keyRaw = clg::get_from_lua_raw<int>(l, -2);
                if (keyRaw.is_error()) {
                    lua_pop(l, 2);
                    return keyRaw.error();
                }
                auto key = *keyRaw - 1;
                auto value = clg::get_from_lua<clg::ref>(l, -1);
                lua_pop(l, 1);
                if (key == result.size()) {
                    result.push_back(std::move(value));
                    continue;
                }
                if (key > result.size()) {
                    result.resize(key + 1);
                }
                result[key] = std::move(value);
            }
            return result;
        }

        static int to_lua(lua_State* l, const table_array& v) {
            lua_createtable(l, v.size(), 0);

            for (unsigned i = 0; i < v.size(); ++i) {
                v[i].push_value_to_stack();
                lua_rawseti(l, -2, i + 1);
            }
            return 1;
        }
    };

}
