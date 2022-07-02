/* This file is part of the the dci project. Copyright (C) 2013-2022 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "statsIA.hpp"

namespace dci::module::ppn::connectivity::reest
{
    const KeyIA& stat2Key(const StatIA& stat)
    {
        union U
        {
            int _stub;
            const StatIARecord _rec;

            U() : _stub{} {}
            ~U() {}
        } u{};

        const char* p1 = static_cast<const char*>(static_cast<const void*>(&u._rec.first));
        const char* p2 = static_cast<const char*>(static_cast<const void*>(&u._rec.second));
        std::size_t offset = p2-p1;

        const char* p = static_cast<const char*>(static_cast<const void*>(&stat));
        p = p - offset;

        return *static_cast<const KeyIA*>(static_cast<const void*>(p));
    }
}
