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

#include <grpc++/grpc++.h>
#include <chrono>
#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "snap/rpc/plugin.grpc.pb.h"
#include "snap/rpc/plugin.pb.h"

#include "snap/proxy/plugin_proxy.h"

namespace Plugin {
    namespace Proxy {
        class StreamCollectorImpl final : public rpc::StreamCollector::Service {
        public:
            explicit StreamCollectorImpl(Plugin::StreamCollectorInterface* plugin);

            ~StreamCollectorImpl();

            grpc::Status GetMetricTypes(grpc::ServerContext* context,
                                        const rpc::GetMetricTypesArg* request,
                                        rpc::MetricsReply* resp);

            grpc::Status SetConfig();

            grpc::Status Kill(grpc::ServerContext* context, const rpc::KillArg* request,
                            rpc::ErrReply* response);

            grpc::Status GetConfigPolicy(grpc::ServerContext* context,
                                        const rpc::Empty* request,
                                        rpc::GetConfigPolicyReply* resp);

            grpc::Status Ping(grpc::ServerContext* context, const rpc::Empty* request,
                            rpc::ErrReply* resp);

            grpc::Status StreamMetrics(grpc::ServerContext* context,
                            grpc::ServerReaderWriter<rpc::CollectReply, rpc::CollectArg>* stream);

            void SetMaxCollectDuration(std::chrono::seconds maxCollectDuration) {
                _max_collect_duration = maxCollectDuration;
            }
            std::chrono::seconds GetMaxCollectDuration () const {
                return _max_collect_duration;
            }
            void SetMaxMetricsBuffer(size_t maxMetricsBuffer) {
                _max_metrics_buffer = maxMetricsBuffer;
            }
            size_t GetMaxMetricsBuffer() const {
                return _max_metrics_buffer;
            }

            bool sendAndClearMetricsReply();
            void receiveReply(const rpc::CollectArg* reply);
            bool streamRecv();

            /**
             * sendMetrics (and get_rpc_metric below) have three specializations
             * as defined by caller functions in plugin.h: T = vector<Plugin::Metric>,
             * T = vector<Plugin::Metric*> and T = vector<rpc::Metric*>
             */
            template<typename T>
            void sendMetrics(const std::vector<T>& metrics);
            void sendErrorMessage(const std::string& msg);
            bool contextCancelled();


        private:
            Plugin::StreamCollectorInterface* _stream_collector;
            PluginImpl* _plugin_impl_ptr;
            grpc::ServerContext* _ctx;
            size_t _max_metrics_buffer;
            std::chrono::seconds _max_collect_duration;
            rpc::CollectReply _collect_reply;
            rpc::MetricsReply *_metrics_reply;
            rpc::ErrReply *_err_reply;
            size_t _copied_metrics_count;
            std::chrono::steady_clock::time_point _collect_duration_start;

            grpc::ServerContext* _current_context;
            grpc::ServerReaderWriter<rpc::CollectReply, rpc::CollectArg>* _current_stream;

            template<typename T>
            rpc::Metric* get_rpc_metric(const T& met) const;
        };
    }  // namespace Proxy
}  // namespace Plugin
