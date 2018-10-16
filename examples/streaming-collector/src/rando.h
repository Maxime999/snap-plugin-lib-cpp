/*
http://www.apache.org/licenses/LICENSE-2.0.txt
Copyright 2017 Intel Corporation
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once
#include <vector>
#include <iostream>
#include <mutex>

#include <snap/config.h>
#include <snap/metric.h>
#include <snap/plugin.h>
#include <snap/flags.h>


class Rando final : public Plugin::StreamCollectorInterface {
public:
    Rando();

    const Plugin::ConfigPolicy get_config_policy() override final;

    std::vector<Plugin::Metric> get_metric_types(Plugin::Config cfg) override final;
    void get_metrics_in(std::vector<Plugin::Metric> &metsIn) override final {
        std::unique_lock<std::mutex> lock(_m);
        _metrics.clear();
        std::copy(metsIn.begin(), metsIn.end(), std::back_inserter(_metrics));
    }

    void stream_metrics() override final;


private:
    std::vector<Plugin::Metric> _metrics;
    std::mutex _m;
};
