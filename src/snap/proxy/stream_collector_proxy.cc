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
#include <grpc++/grpc++.h>
#include <vector>
#include <chrono>
#include <iostream>
#include <future>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "snap/proxy/stream_collector_proxy.h"
#include "snap/rpc/plugin.pb.h"
#include "snap/metric.h"

using google::protobuf::RepeatedPtrField;

using grpc::Server;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using grpc::StatusCode;

using rpc::Empty;
using rpc::ErrReply;
using rpc::GetConfigPolicyReply;
using rpc::GetMetricTypesArg;
using rpc::KillArg;
using rpc::MetricsArg;
using rpc::MetricsReply;
using rpc::CollectArg;
using rpc::CollectReply;

using Plugin::Metric;
using Plugin::PluginException;
using Plugin::Proxy::StreamCollectorImpl;


StreamCollectorImpl::StreamCollectorImpl(Plugin::StreamCollectorInterface* plugin) :
                                        _stream_collector(plugin) {
    _plugin_impl_ptr = new PluginImpl(plugin);
    _metrics_reply = new rpc::MetricsReply();
    _err_reply = new rpc::ErrReply();
    _collect_reply.set_allocated_metrics_reply(_metrics_reply);
    _collect_reply.set_allocated_error(_err_reply);
    _copied_metrics_count = 0;
    _max_collect_duration = plugin->GetMaxCollectDuration();
    _max_metrics_buffer = plugin->GetMaxMetricsBuffer();
}

StreamCollectorImpl::~StreamCollectorImpl() {
    delete _plugin_impl_ptr;
}

Status StreamCollectorImpl::GetMetricTypes(ServerContext* context,
                                    const GetMetricTypesArg* req,
                                    MetricsReply* resp) {
    Plugin::Config cfg(const_cast<rpc::ConfigMap&>(req->config()));
    try {
        std::vector<Metric> metrics = _stream_collector->get_metric_types(cfg);

        for (Metric met : metrics) {
            met.set_timestamp();
            met.set_last_advertised_time();
            *resp->add_metrics() = *met.get_rpc_metric_ptr();
        }
        return Status::OK;
    } catch (PluginException &e) {
        resp->set_error(e.what());
        return Status(StatusCode::UNKNOWN, e.what());
    }
}

Status SetConfig() {
    return Status::OK;
}

Status StreamCollectorImpl::Kill(ServerContext* context, const KillArg* req,
                        ErrReply* resp) {
    return _plugin_impl_ptr->Kill(context, req, resp);
}

Status StreamCollectorImpl::GetConfigPolicy(ServerContext* context, const Empty* req,
                                    GetConfigPolicyReply* resp) {
    try {
        return _plugin_impl_ptr->GetConfigPolicy(context, req, resp);
    } catch (PluginException &e) {
        resp->set_error(e.what());
        return Status(StatusCode::UNKNOWN, e.what());
    }
}

Status StreamCollectorImpl::Ping(ServerContext* context, const Empty* req,
                        ErrReply* resp) {
    return _plugin_impl_ptr->Ping(context, req, resp);
}

Status StreamCollectorImpl::StreamMetrics(ServerContext* context,
                ServerReaderWriter<CollectReply, CollectArg>* stream) {
    try {
        _current_context = context;
        _current_stream = stream;
        _stream_collector->_stream_collector_impl = this;
        _max_collect_duration = _stream_collector->GetMaxCollectDuration();
        _max_metrics_buffer = _stream_collector->GetMaxMetricsBuffer();

        CollectArg collectMets;
        do {
            _current_stream->Read(&collectMets);
            receiveReply(&collectMets);
        } while (!collectMets.has_metrics_arg());

        auto recvch = std::async(std::launch::async, &StreamCollectorImpl::streamRecv, this);
        _collect_duration_start = std::chrono::steady_clock::now();

        _stream_collector->stream_metrics();

        _stream_collector->_stream_collector_impl = nullptr;
        _current_context = nullptr;
        _current_stream = nullptr;

        _metrics_reply->mutable_metrics()->ExtractSubrange(_copied_metrics_count, _metrics_reply->metrics_size() - _copied_metrics_count, nullptr);
        _metrics_reply->clear_metrics();
        _copied_metrics_count = 0;

        return Status::OK;
    } catch(PluginException &e) {
        return Status(StatusCode::UNKNOWN, e.what());
    }
}


template<>
rpc::Metric* StreamCollectorImpl::get_rpc_metric(const Plugin::Metric& met) const {
    return met.get_rpc_metric_ptr();
}

template<>
rpc::Metric* StreamCollectorImpl::get_rpc_metric(Plugin::Metric* const& met) const {
    return met->get_rpc_metric_ptr();
}

template<>
rpc::Metric* StreamCollectorImpl::get_rpc_metric(rpc::Metric* const& met) const {
    return met;
}

template<typename T>
void StreamCollectorImpl::sendMetrics(const std::vector<T>& metrics) {
    // Metrics are sent synchronously, so we try ot to copy them as little sa possible
    if (!_current_context->IsCancelled()) {
        // If _max_metrics_buffer == 0 we send all metrics we have directly
        if (_max_metrics_buffer == 0) {
            for (const T& met : metrics)
                _metrics_reply->mutable_metrics()->AddAllocated(get_rpc_metric(met));
            sendAndClearMetricsReply();
        }
        // Otherwise we first send as much as possible chunks of #_max_metrics_buffer metrics
        else {
            size_t index = 0;
            size_t chunk_count = (_metrics_reply->metrics_size() + metrics.size()) / _max_metrics_buffer;
            for (size_t i = 0; i < chunk_count; i++) {
                while (_metrics_reply->metrics_size() < _max_metrics_buffer) {
                    _metrics_reply->mutable_metrics()->AddAllocated(get_rpc_metric(metrics[index]));
                    index++;
                }
                sendAndClearMetricsReply();
            }

            // If _max_collect_duration timer has expired we send the remaining metrics directly
            if (std::chrono::steady_clock::now() - _collect_duration_start >= _max_collect_duration) {
                while (index < metrics.size()) {
                    _metrics_reply->mutable_metrics()->AddAllocated(get_rpc_metric(metrics[index]));
                    index++;
                }
                sendAndClearMetricsReply();
            }
            // Otherwise we copy (and take ownership of) the remaning metrics to be sent later
            else {
                while (index < metrics.size()) {
                    *_metrics_reply->add_metrics() = *get_rpc_metric(metrics[index]);
                    _copied_metrics_count++;
                    index++;
                }
            }
        }
    }
}

template void StreamCollectorImpl::sendMetrics(const std::vector<Plugin::Metric>& metrics);
template void StreamCollectorImpl::sendMetrics(const std::vector<Plugin::Metric*>& metrics);
template void StreamCollectorImpl::sendMetrics(const std::vector<rpc::Metric*>& metrics);


void StreamCollectorImpl::sendErrorMessage(const std::string& msg) {
    try {
        if (!_current_context->IsCancelled()) {
            _err_reply->set_error(msg);
            _current_stream->Write(_collect_reply);
        }
    } catch (PluginException &e) {
        std::cout << "Error" << std::endl;
    }
}

bool StreamCollectorImpl::contextCancelled() {
    return _current_context->IsCancelled();
}


bool StreamCollectorImpl::sendAndClearMetricsReply() {
    if (_metrics_reply->metrics_size() == 0)
        return true;
    bool success = false;
    try {
        success = _current_stream->Write(_collect_reply);
    } catch (PluginException &e) {
        success = false;
        std::cout << "Error" << std::endl;
    }
    // Clear _metrics_reply:
    // metrics which were copied (at the beginning of the array) are released,
    // metrics which were not copied are simply extracted (they are owned by the plugin)
    _metrics_reply->mutable_metrics()->ExtractSubrange(_copied_metrics_count, _metrics_reply->metrics_size() - _copied_metrics_count, nullptr);
    _metrics_reply->clear_metrics();
    _copied_metrics_count = 0;
    _collect_duration_start = std::chrono::steady_clock::now();
    return success;
}

void StreamCollectorImpl::receiveReply(const rpc::CollectArg* reply) {
    if (reply->maxcollectduration() > 0) {
        _max_collect_duration = std::chrono::seconds(reply->maxcollectduration());
        _stream_collector->SetMaxCollectDuration(_max_collect_duration);
    }
    if (reply->maxmetricsbuffer() > 0) {
        _max_metrics_buffer = reply->maxmetricsbuffer();
        _stream_collector->SetMaxMetricsBuffer(_max_metrics_buffer);
    }

    if (reply->has_metrics_arg()) {
        std::vector<Metric> recv_mets;
        RepeatedPtrField<rpc::Metric> rpc_mets = reply->metrics_arg().metrics();

        for (int i = 0; i < rpc_mets.size(); i++) {
            recv_mets.emplace_back(rpc_mets.Mutable(i));
        }
        _stream_collector->get_metrics_in(recv_mets);
    }
}

bool StreamCollectorImpl::streamRecv() {
    try {
        while (!_current_context->IsCancelled()) {
            CollectArg collectMets;
            _current_stream->Read(&collectMets);
            receiveReply(&collectMets);
       }
        return true;
    } catch (PluginException &e) {
        std::cout << "Error" << std::endl;
        return false;
    }
}
