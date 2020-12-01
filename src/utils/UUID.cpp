/*
 * Copyright (C) 2020 Emeric Poupon
 *
 * This file is part of fileshelter.
 *
 * fileshelter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fileshelter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fileshelter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils/UUID.hpp"

#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

#include <regex>

#include "utils/String.hpp"

namespace StringUtils
{
	template <>
	std::optional<UUID>
	readAs(const std::string& str)
	{
		return UUID::fromString(str);
	}
}

static
bool
stringIsUUID(std::string_view str)
{
	static const std::regex re { R"([0-9a-fA-F]{8}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{12})"};

	return std::regex_match(std::cbegin(str), std::cend(str), re);
}

UUID
UUID::generate()
{
	boost::uuids::uuid uuid {boost::uuids::random_generator()()};

	std::ostringstream oss;
	oss << uuid;

	return UUID {oss.str()};
}

UUID::UUID(std::string_view str)
: _value {StringUtils::stringToLower(str)}
{
}

std::optional<UUID>
UUID::fromString(std::string_view str)
{
	if (!stringIsUUID(str))
		return std::nullopt;

	return UUID {str};
}

