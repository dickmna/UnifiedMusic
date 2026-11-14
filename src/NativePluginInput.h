#pragma once
#include "core/PluginRecords.h"
#include <algorithm>
#include <stdexcept>

namespace UnifiedMusic {
namespace NativeInput {
inline std::string Tag(std::uint32_t value) {
    const char bytes[]{char(value),char(value>>8),char(value>>16),char(value>>24)};
    return {bytes,4};
}
inline bool SameName(std::string_view a,std::string_view b) {
    const auto lower=[](unsigned char c) { return c>='A' && c<='Z'?c+('a'-'A'):c; };
    return std::ranges::equal(a,b,[&](auto x,auto y) {return lower(x)==lower(y);});
}

// On 1.7.104 the engine remaps record-header IDs, but leaves subrecord IDs
// file-local. Reverse only the header, using the already loaded master table.
template<class File>
std::uint32_t SourceID(const File& file,std::uint32_t runtimeID) {
    if(!runtimeID || file.masterCount>255 || (file.masterCount && !file.masterPtrs))
        throw std::runtime_error("invalid native master table or record ID");
    for(std::uint32_t i=0;i<=file.masterCount;++i) {
        const auto* owner=i==file.masterCount?&file:file.masterPtrs[i];
        if(!owner) throw std::runtime_error("missing native master");
        if(owner->IsLight()) {
            if((runtimeID>>24)==0xFE && ((runtimeID>>12)&0xFFF)==owner->GetSmallFileCompileIndex())
                return (i<<24)|(runtimeID&0xFFF);
        } else if((runtimeID>>24)==owner->GetCompileIndex()) {
            return (i<<24)|(runtimeID&0xFFFFFF);
        }
    }
    throw std::runtime_error("native record ID has no source master");
}

// A narrow template lets offline contract tests exercise this exact traversal
// against an engine-API double. It contains no disk reader or decompressor.
template<class File,class OpenRead,class NextForm>
void Read(File& file,OpenRead openRead,NextForm nextForm,const RecordConsumer& consume) {
    if(file.lockedFile) throw std::runtime_error("native plugin has a locked writer");
    if(!openRead(file)) throw std::runtime_error("native plugin open failed");
    struct Close {
        File* file;
        ~Close() {if(file) try {file->CloseTES(false);} catch(...) {}}
    } close{&file};
    const std::uint64_t length=file.filesize;
    if(length<24 || length>(2ULL<<30)) throw std::runtime_error("invalid native plugin size");
    std::vector<std::uint64_t> groups;
    std::uint64_t expected=0;
    do {
        const std::uint64_t position=file.fileOffset;
        if(position!=expected) throw std::runtime_error("native record traversal skipped data");
        while(!groups.empty() && position==groups.back()) groups.pop_back();
        const auto limit=groups.empty()?length:groups.back();
        if(position>limit || limit-position<24) throw std::runtime_error("invalid native record boundary");
        const auto header=file.currentform;
        const auto type=Tag(header.form);
        if(!position && type!="TES4") throw std::runtime_error("missing native TES4 header");
        if(type=="GRUP") {
            if(header.length<24 || header.length>limit-position || groups.size()>=32)
                throw std::runtime_error("invalid native group boundary");
            groups.push_back(position+header.length);
            expected=position+24;
        } else {
            if(header.length>limit-position-24) throw std::runtime_error("invalid native record size");
            expected=position+24+header.length;
            PluginRecord record{type,header.flags,0,{}};
            if(PluginRecordNeedsID(type) && !(header.flags&(0x4000|0x1000)))
                record.id=SourceID(file,header.formID);
            const bool compressed=(header.flags&0x40000)!=0;
            if(compressed && header.length<4) throw std::runtime_error("invalid native compressed record");
            std::vector<std::vector<std::uint8_t>> storage;
            if(header.length) for(;;) {
                const auto tag=file.GetCurrentSubRecordType();
                const std::uint64_t size=compressed?file.decompressedFormBufferSize:header.length;
                if(compressed && (!file.decompressedFormBuffer || size>128*1024*1024))
                    throw std::runtime_error("native decompression unavailable or oversized");
                if(!size && !tag) break;
                const std::uint64_t offset=file.formoffset, count=file.actualChunkSize;
                if(!tag || offset>size || size-offset<6 || count>size-offset-6)
                    throw std::runtime_error("invalid native subrecord boundary");
                const auto field=Tag(tag);
                if(NeedsPluginField(type,field)) {
                    // A non-null backing pointer also represents zero-length fields.
                    auto& bytes=storage.emplace_back(std::max<std::size_t>(count,1));
                    if(count && !file.ReadData(bytes.data(),static_cast<std::uint32_t>(count)))
                        throw std::runtime_error("native subrecord read failed");
                    record.fields.emplace_back(field,PluginBytes(bytes.data(),count));
                }
                const auto end=offset+6+count;
                if(!file.SeekNextSubrecord()) {
                    if(end!=size || file.formoffset!=size)
                        throw std::runtime_error("native subrecord traversal ended early");
                    break;
                }
                // XXXX is expanded by the engine, and can advance beyond end.
                if(file.formoffset<end || file.formoffset>=size)
                    throw std::runtime_error("native subrecord traversal made invalid progress");
            }
            if(type=="TES4") {
                std::uint32_t index=0;
                for(const auto& [field,value]:record.fields) if(field=="MAST") {
                    const std::string name(value.begin(),std::find(value.begin(),value.end(),0));
                    if(index>=file.masterCount || !file.masterPtrs || !file.masterPtrs[index] ||
                        !SameName(name,file.masterPtrs[index]->GetFilename()))
                        throw std::runtime_error("native master table differs from plugin header");
                    ++index;
                }
                if(index!=file.masterCount) throw std::runtime_error("incomplete native master table");
            }
            consume(record);
        }
    } while(nextForm(file));
    if(file.fileOffset!=length || expected!=length || file.currentform.form)
        throw std::runtime_error("native record traversal ended early");
    close.file=nullptr;
    if(!file.CloseTES(false)) throw std::runtime_error("native plugin close failed");
}
}
}
