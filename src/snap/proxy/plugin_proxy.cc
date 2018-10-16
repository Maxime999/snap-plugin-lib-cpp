/*
http://www.apache.org/licenses/LICENSE-2.0.txt
Copyright 2016 Intel Corporation
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
#include <grpc++/grpc++.h>
#include <iostream>
#include <thread>

#include "snap/proxy/plugin_proxy.h"
#include "snap/rpc/plugin.pb.h"

using grpc::Server;
using grpc::ServerContext;
using grpc::Status;

using rpc::Empty;
using rpc::ErrReply;
using rpc::KillArg;
using rpc::GetConfigPolicyReply;

using Plugin::Proxy::PluginImpl;

std::promise<void> PluginImpl::pluginKilled{};
std::shared_future<void> PluginImpl::pluginKilledLock{PluginImpl::pluginKilled.get_future()};

PluginImpl::PluginImpl(Plugin::PluginInterface* plugin) : plugin(plugin) {}

Status PluginImpl::Ping(ServerContext* context, const Empty* req,
                        ErrReply* resp) {
    _lastPing = std::chrono::system_clock::now();
    // Change to log
    std::cerr << "Heartbeat received at: " << _lastPing << std::endl;
    if (!_heartbeatWatcher.valid() || _heartbeatWatcher.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        _heartbeatWatcher = std::async(std::launch::async, &PluginImpl::HeartbeatWatch, this);
    return Status::OK;
}

Status PluginImpl::Kill(ServerContext* context, const KillArg* req,
                        ErrReply* resp) {
    std::cerr << "Kill received at: " << _lastPing << std::endl;
    PluginImpl::pluginKilled.set_value();
    return Status::OK;
}

Status PluginImpl::GetConfigPolicy(ServerContext* context, const Empty* req,
                                   GetConfigPolicyReply* resp) {
    *resp = plugin->get_config_policy();
    return Status::OK;
}

void PluginImpl::HeartbeatWatch() {
    _lastPing = std::chrono::system_clock::now();
    std::cerr << "Heartbeat watcher started" << std::endl;
    int count = 0;
    std::this_thread::sleep_for(_pingTimeoutDuration / 2);
    while (true) {
        if ((std::chrono::system_clock::now() - _lastPing) >= _pingTimeoutDuration) {
            ++count;
            std::cerr << "Heartbeat timeout " << count
                        << " of " << _pingTimeoutLimit
                        << ".  (Duration between checks " << _pingTimeoutDuration.count() << ")" << std::endl;
            if (count >= _pingTimeoutLimit) {
                std::cerr << "Heartbeat timeout expired!" << std::endl;
                PluginImpl::pluginKilled.set_value();
                return;
            }
        } else if (count > 0) {
            std::cerr << "Heartbeat timeout reset" << std::endl;
            count = 0;
        }
        std::this_thread::sleep_for(_pingTimeoutDuration);
    }
}

std::shared_future<void> PluginImpl::getPluginKilledLock() {
    return PluginImpl::pluginKilledLock;
}
