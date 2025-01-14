#include <pingcap/Exception.h>
#include <pingcap/kv/Backoff.h>
#include <pingcap/kv/Scanner.h>
#include <pingcap/kv/Snapshot.h>

namespace pingcap
{
namespace kv
{

constexpr int scan_batch_size = 256;

//bool extractLockFromKeyErr()

std::string Snapshot::Get(const std::string & key)
{
    Backoffer bo(GetMaxBackoff);
    for (;;)
    {
        auto location = cache->locateKey(bo, key);
        auto regionClient = RegionClient(cache, client, location.region);
        auto request = new kvrpcpb::GetRequest();
        request->set_key(key);
        request->set_version(version);

        auto context = request->mutable_context();
        context->set_priority(::kvrpcpb::Normal);
        context->set_not_fill_cache(false);
        auto rpc_call = std::make_shared<RpcCall<kvrpcpb::GetRequest>>(request);

        try
        {
            regionClient.sendReqToRegion(bo, rpc_call);
        }
        catch (Exception & e)
        {
            cache->dropRegion(location.region);
            bo.backoff(boRegionMiss, e);
            continue;
        }
        auto response = rpc_call->getResp();
        if (response->has_error())
        {
            throw Exception("has key error", LockError);
        }
        return response->value();
    }
}

Scanner Snapshot::Scan(const std::string & begin, const std::string & end) { return Scanner(*this, begin, end, scan_batch_size); }

} // namespace kv
} // namespace pingcap
