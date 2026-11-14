#include "PluginScan.h"
#include <span>
#include <stdexcept>

namespace UnifiedMusic {
namespace {
struct Reader {
    std::span<const std::uint8_t> data;
    std::size_t pos{};
    void skip(std::size_t n) {
        if(n>data.size()-pos) throw std::runtime_error("truncated VMAD");
        pos+=n;
    }
    std::uint32_t number(unsigned n) {
        const auto start=pos;skip(n);std::uint32_t value=0;
        for(unsigned i=0;i<n;++i) value|=std::uint32_t(data[start+i])<<(8*i);
        return value;
    }
    void string() {skip(number(2));}
};
}

void DecodeScriptProperties(ScriptProperties& script,const std::string& recordType) {
    try {
        Reader r{script.bytes};
        const auto version=r.number(2), format=r.number(2);
        if((version!=4 && version!=5) || (format!=1 && format!=2)) return;
        std::set<FormKey> references;
        const auto object=[&](unsigned fmt,bool keep) {
            std::uint32_t raw;
            if(fmt==2) {r.skip(4);raw=r.number(4);} else {raw=r.number(4);r.skip(4);}
            if(!raw || !keep) return;
            const auto index=raw>>24;
            if(index>script.masters.size()) throw std::runtime_error("invalid VMAD object");
            references.insert({index==script.masters.size()?script.source:script.masters[index],raw&0xffffff});
        };
        const auto scripts=[&](unsigned fmt) {
            const auto count=r.number(2);
            for(unsigned i=0;i<count;++i) {
                r.string();const auto flags=r.number(1), properties=r.number(2);
                for(unsigned j=0;j<properties;++j) {
                    r.string();auto type=r.number(1);const auto status=r.number(1);
                    std::uint32_t items=1;
                    if(type>=11 && type<=15) {type-=10;items=r.number(4);}
                    if(items>100000 || type>5) throw std::runtime_error("unsupported VMAD value");
                    for(unsigned k=0;k<items;++k) {
                        switch(type) {
                        case 0: break;
                        case 1: object(fmt,!(flags&2) && status!=3);break;
                        case 2: r.string();break;
                        case 3: case 4: r.skip(4);break;
                        case 5: r.skip(1);break;
                        }
                    }
                }
            }
        };
        scripts(format);
        if(recordType=="QUST" && r.pos<r.data.size()) {
            if(r.number(1)!=2) throw std::runtime_error("unsupported quest VMAD");
            const auto count=r.number(2);r.string();
            for(unsigned i=0;i<count;++i) {r.skip(9);r.string();r.string();}
            if(r.pos<r.data.size()) {
                const auto aliases=r.number(2);
                for(unsigned i=0;i<aliases;++i) {
                    object(format,false);
                    const auto av=r.number(2), af=r.number(2);
                    if((av!=4 && av!=5) || (af!=1 && af!=2)) throw std::runtime_error("unsupported alias VMAD");
                    scripts(af);
                }
            }
            if(r.pos!=r.data.size()) throw std::runtime_error("unknown quest VMAD trailer");
        }
        // Other TES5 fragment trailers contain script/function names, not
        // another property table. This parser does not infer script calls.
        script.references=std::move(references);script.parsed=true;
        script.bytes.clear();script.bytes.shrink_to_fit();
    } catch(const std::exception&) {
        // Unknown/truncated structures retain the conservative byte fallback;
        // valid integers/strings can no longer masquerade as object properties.
    }
}
}
