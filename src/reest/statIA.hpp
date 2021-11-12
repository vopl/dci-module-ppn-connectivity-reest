/* This file is part of the the dci project. Copyright (C) 2013-2021 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#pragma once

#include "pch.hpp"
#include "keyIA.hpp"
#include "statIA/recorder.hpp"

namespace dci::module::ppn::connectivity
{
    class Reest;
}

namespace dci::module::ppn::connectivity::reest
{
    class StatIA
    {
    public:
        StatIA(Reest* srv);
        ~StatIA();

    public:
        struct SessionState
        {
            sbs::Owner          _sbsOwner;
            statIA::TimePoint   _startMoment{};
            statIA::TimePoint   _lastFlushMoment{};
            bool                _connected {};
            bool                _joined {};
        };

    public:
        void discovered();
        void newSession(node::feature::CSession<> s);
        void rekeyed(node::feature::CSession<> s, const SessionState& ssFrom);
        void regularFlush();
        real64 rating() const;
        bool dead() const;

    private:
        static real64 flushOnline(SessionState& ss, statIA::TimePoint m);
        void updateResult();

    private:
        Reest* _srv;
        real64 _rating {};

    private:
        static constexpr real64 _costDiscover       {1.0};
        static constexpr real64 _costConnectionFail {-10.0};
        static constexpr real64 _costJoinFail       {-20.0};
        static constexpr real64 _costFail           {-10.0};
        static constexpr real64 _costOffline        {-0.01};

        statIA::Recorder _recorder;

        using Sessions = std::map<node::feature::CSession<>, SessionState>;
        Sessions _sessions;
    };
}
