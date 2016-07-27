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
#include "snap/plugin.h"

#include <grpc++/grpc++.h>

#include <chrono>
#include <iostream>
#include <string>

#include <json.hpp>

#include "snap/rpc/plugin.pb.h"

#include "snap/proxy/collector_proxy.h"

using std::cout;
using std::endl;

using grpc::Server;
using grpc::ServerBuilder;

using json = nlohmann::json;

static void emit_preamble(const Plugin::Meta& meta) {
  json j = {
    {"Meta", {
      {"Type", meta.type},
      {"Name", meta.name},
      {"Version", meta.version},
      {"RPCType", meta.rpc_type},
      {"ConcurrencyCount", meta.concurrency_count},
      {"Exclusive", meta.exclusive},
      {"Unsecure", meta.unsecure},
      {"CacheTTL", meta.cache_ttl.count()},
      {"RoutingStrategy", meta.strategy}
    }},
    {"ListenAddress", "127.0.0.1:9997"},
    {"Type", meta.type},
    {"State", 0},
    {"ErrMessage", ""},
    {"Version", meta.version},
  };
  cout << j << endl;
}

Plugin::Meta::Meta(Type type, std::string name, int version) :
                   type(type),
                   name(name),
                   version(version),
                   rpc_type(RpcType::GRPC),
                   concurrency_count(5),
                   exclusive(false),
                   unsecure(false),
                   cache_ttl(std::chrono::milliseconds(500)),
                   strategy(Strategy::LRU) {}

void Plugin::start_collector(CollectorInterface* collector,
                             const Meta& meta) {
    emit_preamble(meta);

    // TODO(danielscottt): Port selection!
    std::string server_address("0.0.0.0:9997");
    Proxy::CollectorImpl collector_impl(collector);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&collector_impl);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
}