#include "PluginFile.h"
#include "core/PluginScan.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <zlib.h>

namespace UnifiedMusic {
namespace {
using Bytes = PluginBytes;
std::uint32_t U32(Bytes b, size_t i) {
    if (i > b.size() || b.size()-i < 4) throw std::runtime_error("truncated uint32");
    return b[i] | (std::uint32_t(b[i+1])<<8) | (std::uint32_t(b[i+2])<<16) | (std::uint32_t(b[i+3])<<24);
}
std::vector<std::pair<std::string, Bytes>> Subs(Bytes b) {
    std::vector<std::pair<std::string, Bytes>> result;
    size_t i=0; std::uint32_t extended=0; bool hasExtended=false;
    while (i < b.size()) {
        if (b.size()-i < 6) throw std::runtime_error("truncated subrecord");
        std::string sig(reinterpret_cast<const char*>(b.data()+i),4);
        size_t n=b[i+4] | (unsigned(b[i+5])<<8); i+=6;
        if (sig=="XXXX") {
            if(n!=4 || hasExtended) throw std::runtime_error("invalid XXXX");
            extended=U32(b,i); hasExtended=true; i+=4; continue;
        }
        if (hasExtended) { n=extended; hasExtended=false; }
        if (n>b.size()-i) throw std::runtime_error("subrecord exceeds record");
        result.emplace_back(sig,b.subspan(i,n)); i+=n;
    }
    if(hasExtended) throw std::runtime_error("orphan XXXX");
    return result;
}
}

void ReadPluginRecords(const std::filesystem::path& path, const RecordConsumer& consume) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream) throw std::runtime_error("cannot open plugin: "+path.string());
    auto length=stream.tellg();
    if(length<24 || length>std::streamoff(2ULL<<30)) throw std::runtime_error("invalid plugin size");
    std::vector<std::uint8_t> data(static_cast<size_t>(length)); stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(data.data()),length)) throw std::runtime_error("short plugin read");
    if(std::memcmp(data.data(),"TES4",4)!=0) throw std::runtime_error("missing TES4 header");
    std::function<void(size_t,size_t,unsigned)> walk = [&](size_t start,size_t end,unsigned depth) {
        if(depth>32) throw std::runtime_error("plugin group depth exceeded");
        for(size_t p=start;p<end;) {
            if(end-p<24) throw std::runtime_error("truncated record header");
            std::string sig(reinterpret_cast<char*>(data.data()+p),4);
            size_t n=U32(data,p+4);
            if(sig=="GRUP") {
                if(n<24 || n>end-p) throw std::runtime_error("invalid group boundary");
                walk(p+24,p+n,depth+1); p+=n; continue;
            }
            if(n>end-p-24) throw std::runtime_error("invalid record boundary");
            auto flags=U32(data,p+8), id=U32(data,p+12);
            Bytes body=Bytes(data).subspan(p+24,n);
            std::vector<std::uint8_t> expanded;
            if(flags&0x40000) {
                auto expected=U32(body,0);
                if(expected>128*1024*1024) throw std::runtime_error("record decompression limit exceeded");
                expanded.resize(expected); uLongf size=expected;
                if(uncompress(expanded.data(),&size,body.data()+4,static_cast<uLong>(body.size()-4))!=Z_OK || size!=expected)
                    throw std::runtime_error("record decompression failed");
                body=expanded;
            }
            auto subs=Subs(body);
            consume({std::move(sig),flags,id,std::move(subs)});
            p+=24+n;
        }
    };
    walk(0,data.size(),0);
}

void ReadPluginFile(PluginScan& scan, const std::filesystem::path& path) {
    scan.Read(path.filename().string(),[&](const RecordConsumer& consume) {
        ReadPluginRecords(path,consume);
    });
}
}
