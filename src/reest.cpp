/* This file is part of the the dci project. Copyright (C) 2013-2021 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "reest.hpp"

namespace dci::module::ppn::connectivity
{
    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Reest::Reest()
        : api::Reest<>::Opposite(idl::interface::Initializer())
    {
        {
            node::Feature<>::Opposite op = *this;

            op->setup() += sol() * [this](node::feature::Service<> srv)
            {
                srv->start() += sol() * [this, srv]() mutable
                {
                    _started = true;
                    _regularFlushTicker.start();
                };

                srv->stop() += sol() * [this]
                {
                    _regularFlushTicker.stop();
                    _started = false;
                };

                srv->discovered() += sol() * [this](const node::link::Id& id, const transport::Address& a)
                {
                    if(!_started) return;

                    reest::StatIA& stat = _statsIA.try_emplace(std::tie(id, a), this).first->second;
                    stat.discovered();
                };

                node::feature::Connectors<> csrv = srv;

                //out newSession(link::Id, transport::Address, CSession);
                csrv->newSession() += sol() * [this](const node::link::Id& id, const transport::Address& a, const node::feature::CSession<>& s) mutable
                {
                    reest::StatIA& stat = _statsIA.try_emplace(std::tie(id, a), this).first->second;
                    stat.newSession(s);
                };
            };
        }

        {
            rdb::Feature<>::Opposite op = *this;

            op->setup() += sol() * [](const rdb::feature::Service<>& srv)
            {
                (void)srv;
                return cmt::readyFuture(List<rdb::pql::Column>{
                                            {rdb::pql::Column{api::Reest<>::lid(), "topAddresses"}},
                                            {rdb::pql::Column{api::Reest<>::lid(), "address"}},
                                            {rdb::pql::Column{api::Reest<>::lid(), "rating"}}
                                        });
            };
        }

        {
            idl::Configurable<>::Opposite op = *this;

            op->configure() += sol() * [this](dci::idl::Config&& config)
            {
                auto c = config::cnvt(std::move(config));

                setIntensity(std::atof(c.get("intensity", "0.016").data()));

                return cmt::readyFuture<>();
            };
        }
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Reest::~Reest()
    {
        _started = false;

        _statsIA.clear();

        _regularFlushTicker.stop();
        _regularFlushStats.clear();

        _onceFlushTicker.stop();
        _onceFlushStats.clear();

        _statsI.clear();

        sol().flush();
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::subscribeRegularFlush(reest::StatIA* stat)
    {
        _regularFlushStats.insert(stat);
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::unsubscribeRegularFlush(reest::StatIA* stat)
    {
        _regularFlushStats.erase(stat);
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::wantOnceFlush(reest::StatI* stat)
    {
        _onceFlushStats.insert(stat);
        _onceFlushTicker.start();
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::rekeyed(node::feature::CSession<> s, const reest::StatIA::SessionState& ss, const node::link::Id& id, const transport::Address& a)
    {
        reest::StatIA& stat = _statsIA.emplace(std::tie(id, a), this).first->second;
        stat.rekeyed(s, ss);
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::statChanged(reest::StatIA* stat)
    {
        if(stat->dead())
        {
            reest::StatsI::iterator statsIIter = _statsI.find(std::get<0>(reest::stat2Key(*stat)));
            if(_statsI.end() != statsIIter)
            {
                reest::StatI& statI = statsIIter->second;
                statI.statDead(stat);
            }

            _statsIA.erase(reest::stat2Key(*stat));
            return;
        }

        reest::StatI& statI = _statsI.emplace(std::get<0>(reest::stat2Key(*stat)), this).first->second;
        statI.statChanged(stat);
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::statChanged(reest::StatI* stat)
    {
        const std::vector<reest::StatIA*>& top = stat->top();

        if(top.empty())
        {
            methods()->updateRecord(reest::stat2Key(*stat), List<rdb::pql::Value>{
                                   rdb::pql::Value{},
                                   rdb::pql::Value{},
                                   rdb::pql::Value{}});

            if(stat->dead())
            {
                _onceFlushStats.erase(stat);
                _statsI.erase(reest::stat2Key(*stat));
            }

            return;
        }

        List<rdb::pql::Value> topAddresses;
        for(const reest::StatIA* ia : top)
        {
            topAddresses.push_back(rdb::pql::Value{std::get<1>(reest::stat2Key(*ia)).value});
        }

        String address = std::get<1>(reest::stat2Key(*top.front())).value;
        real64 rating = top.front()->rating();

        methods()->updateRecord(reest::stat2Key(*stat), List<rdb::pql::Value>{
                               rdb::pql::Value{std::move(topAddresses)},
                               rdb::pql::Value{std::move(address)},
                               rdb::pql::Value{rating}
                           });
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::regularFlush()
    {
        if(!_started)
        {
            return;
        }

        auto iter = _regularFlushStats.begin();
        while(iter != _regularFlushStats.end())
        {
            reest::StatIA* stat = *iter;
            ++iter;
            stat->regularFlush();
        }
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::onceFlush()
    {
        if(!_started)
        {
            return;
        }

        std::set<reest::StatI*> onceFlushStats(std::move(_onceFlushStats));
        for(reest::StatI* stat : onceFlushStats)
        {
            stat->onceFlush();
        }
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Reest::setIntensity(double v)
    {
        static constexpr int64 min = 50;
        static constexpr int64 max = int64{1000}*60*60*24;

        int64 ms;
        if(v <= 0)  ms = max;
        else        ms = std::clamp(static_cast<int64>(1000.0/v), min, max);

        _regularFlushTicker.interval(std::chrono::milliseconds{ms});
    }
}
