#include "AudioAssets.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>

namespace UnifiedMusic {
namespace {
std::uint32_t U32(std::span<const std::uint8_t> b,std::size_t p) {
    if(p>b.size() || b.size()-p<4) throw std::runtime_error("truncated audio integer");
    return b[p]|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);
}
std::uint64_t U64(std::span<const std::uint8_t> b,std::size_t p) {return U32(b,p)|(std::uint64_t(U32(b,p+4))<<32);}
AudioBytes ReadAt(std::ifstream& f,std::uint64_t offset,std::size_t count,std::uint64_t length) {
    if(offset>length || count>length-offset || count>MaxAudioBytes) throw std::runtime_error("audio archive bounds exceeded");
    AudioBytes b(count); f.clear(); f.seekg(static_cast<std::streamoff>(offset));
    if(count && !f.read(reinterpret_cast<char*>(b.data()),count)) throw std::runtime_error("short audio archive read");
    return b;
}
bool Tag(std::span<const std::uint8_t> b,std::size_t p,const char* s) {return p<=b.size() && b.size()-p>=4 && !std::memcmp(b.data()+p,s,4);}
std::string Lower(std::string s) {for(auto& c:s) {if(c=='\\') c='/'; if(c>='A'&&c<='Z') c+=32;} return s;}
}
std::optional<std::string> MusicPath(std::string path) {
    path=Lower(std::move(path));
    if(path.starts_with("/data/")) path.erase(0,1);
    if(path.starts_with("data/")) path.erase(0,5);
    if(!path.starts_with("music/") || path.size()>240 || path.find_first_of(":*?\"<>|")!=std::string::npos) return {};
    std::size_t start=0;
    while(start<path.size()) {
        const auto end=path.find('/',start);const auto part=path.substr(start,end==std::string::npos?end:end-start);
        if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ') return {};
        for(unsigned char c:part) if(c<32 || c>=127) return {};
        if(end==std::string::npos) break;
        start=end+1;
    }
    if(path.ends_with(".wav")) path.replace(path.size()-4,4,".xwm");
    if(!path.ends_with(".xwm")) return {};
    return path;
}
bool ValidMusic(std::span<const std::uint8_t> b) {
    if(b.size()<12 || b.size()>MaxAudioBytes || !Tag(b,0,"RIFF") || !Tag(b,8,"XWMA") || U32(b,4)!=b.size()-8) return false;
    bool format=false,data=false;
    for(std::size_t p=12;p<b.size();) {
        if(b.size()-p<8) return false;
        const auto n=U32(b,p+4); if(n>b.size()-p-8) return false;
        if(Tag(b,p,"fmt ")) {
            if(format||n<16) return false;
            const auto codec=b[p+8]|(unsigned(b[p+9])<<8);
            if((codec!=0x161 && codec!=0x162) || !(b[p+10]|b[p+11]) || !U32(b,p+12)) return false;
            format=true;
        }
        if(Tag(b,p,"data")) {if(data || !n) return false;data=true;}
        p+=8+n+(n&1); if(p>b.size()) return false;
    }
    return format&&data;
}
std::string AudioHash(std::span<const std::uint8_t> bytes) {
    BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char,32> digest{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) throw std::runtime_error("SHA256 provider unavailable");
    auto result=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0);
    if(result>=0) result=BCryptHashData(hash,const_cast<unsigned char*>(bytes.data()),static_cast<ULONG>(bytes.size()),0);
    if(result>=0) result=BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0);
    if(hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg,0);
    if(result<0) throw std::runtime_error("SHA256 failed");
    constexpr char hex[]="0123456789abcdef";std::string out;
    for(auto c:digest) {out+=hex[c>>4];out+=hex[c&15];} return out;
}
std::optional<AudioBytes> ReadAudioFile(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f) return {};
    const auto n=f.tellg();if(n<0 || n>MaxAudioBytes) return {};
    return ReadAt(f,0,static_cast<std::size_t>(n),static_cast<std::uint64_t>(n));
}
void OriginalAudio::AddArchive(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f) throw std::runtime_error("cannot open original audio archive");
    const auto length=static_cast<std::uint64_t>(f.tellg());const auto h=ReadAt(f,0,36,length);
    const auto version=U32(h,4),offset=U32(h,8),flags=U32(h,12),folders=U32(h,16),files=U32(h,20),namesLength=U32(h,28);
    if(!Tag(h,0,"BSA\0") || version!=105 || (flags&3)!=3 || offset<36 || folders>65536 || files>1000000 || namesLength>MaxAudioBytes)
        throw std::runtime_error("unsupported original BSA directory");
    const auto table=ReadAt(f,offset,static_cast<std::size_t>(folders)*24,length);
    struct Row { std::string folder;Entry entry; };
    std::vector<Row> rows;std::uint64_t namesOffset=offset+table.size();
    for(std::size_t i=0;i<folders;++i) {
        const auto count=U32(table,i*24+8);const auto block=U64(table,i*24+16);
        if(count>files-rows.size() || block<namesLength || block-namesLength<offset+table.size()) throw std::runtime_error("invalid BSA folder block");
        auto pos=block-namesLength;const auto size=ReadAt(f,pos++,1,length)[0];
        if(!size) throw std::runtime_error("empty BSA folder name");
        const auto folder=ReadAt(f,pos,size,length);pos+=size;
        if(folder.back()!=0 || std::find(folder.begin(),folder.end()-1,0)!=folder.end()-1) throw std::runtime_error("invalid BSA folder name");
        const std::string dir(folder.begin(),folder.end()-1);
        const auto records=ReadAt(f,pos,static_cast<std::size_t>(count)*16,length);pos+=records.size();
        for(std::size_t j=0;j<count;++j) {
            const auto sizes=U32(records,j*16+8),start=U32(records,j*16+12),n=sizes&0x3fffffff;
            if(start>length || n>length-start) throw std::runtime_error("BSA member outside archive");
            rows.push_back({dir,{path,start,n,bool(flags&4)!=bool(sizes&0x40000000),bool(flags&0x100)}});
        }
        namesOffset=std::max(namesOffset,pos);
    }
    if(rows.size()!=files) throw std::runtime_error("BSA file count mismatch");
    const auto names=ReadAt(f,namesOffset,namesLength,length);std::size_t p=0;
    std::map<std::string,Entry> pending;
    for(const auto& row:rows) {
        const auto end=std::find(names.begin()+p,names.end(),0);
        if(end==names.end()) throw std::runtime_error("truncated BSA filenames");
        const std::string file(names.begin()+p,end);p=static_cast<std::size_t>(end-names.begin())+1;
        const auto raw=Lower(row.folder+"/"+file);
        if(row.entry.offset<namesOffset+namesLength) throw std::runtime_error("BSA audio overlaps directory");
        if(!raw.starts_with("music/")) continue;
        auto normalized=MusicPath(raw);
        if(!normalized || raw!=*normalized || !pending.emplace(*normalized,row.entry).second) throw std::runtime_error("unsafe/duplicate original music member");
    }
    if(p!=names.size()) throw std::runtime_error("unexpected BSA filename bytes");
    for(auto& [key,entry]:pending) {
        if(entries_.contains(key)) {ambiguous_.push_back(key);continue;}
        entries_.emplace(key,std::move(entry));
    }
}
std::optional<AudioBytes> OriginalAudio::Read(const std::string& path) const {
    const auto normalized=MusicPath(path);if(!normalized || std::ranges::find(ambiguous_,*normalized)!=ambiguous_.end()) return {};
    const auto found=entries_.find(*normalized);if(found==entries_.end()) return {};
    const auto& e=found->second;
    // Stock Skyrim music is uncompressed in Sounds.bsa. Never mistake a
    // compressed payload for audio; additional codecs can be added separately.
    if(e.compressed || e.size>MaxAudioBytes) return {};
    std::ifstream f(e.archive,std::ios::binary|std::ios::ate);if(!f) return {};
    auto bytes=ReadAt(f,e.offset,e.size,static_cast<std::uint64_t>(f.tellg()));
    if(e.embedded) {
        if(bytes.empty() || bytes[0]>=bytes.size()) return {};
        bytes.erase(bytes.begin(),bytes.begin()+1+bytes[0]);
    }
    if(!ValidMusic(bytes)) return {};
    return bytes;
}
std::optional<AudioPair> CompareAudio(const OriginalAudio& original,const AudioReader& visible,
    const std::string& track,const std::string& finale,std::string& reason) {
    const auto mainPath=MusicPath(track);const auto endPath=finale.empty()?std::optional<std::string>{}:MusicPath(finale);
    if(!mainPath || (!finale.empty()&&!endPath)) {reason="unsupported music path";return {};}
    AudioPair pair;bool different=false;
    const auto read=[&](const std::string& path,AudioBytes& destination,std::string& hash) {
        auto baseline=original.Read(path);auto current=visible(path);
        if(!baseline || !current || !ValidMusic(*current)) return false;
        different|=*baseline!=*current; destination=std::move(*baseline);hash=AudioHash(destination);return true;
    };
    if(!read(*mainPath,pair.track,pair.trackHash) || (endPath&&!read(*endPath,pair.finale,pair.finaleHash))) {
        reason="missing/invalid original or winning main/finale resource";return {};
    }
    if(!different) {reason="unchanged audio";return {};}
    reason="distinct replacement with complete original resources";return pair;
}
std::string CacheAudio(const std::filesystem::path& root,const AudioBytes& bytes) {
    if(!ValidMusic(bytes)) throw std::runtime_error("invalid cache audio");
    const auto relative="music/harmonia/"+AudioHash(bytes)+".xwm";
    const auto path=root/relative;
    // Check each child before creating anything below it. This rejects junctions
    // as well as symlinks; a manager's Data root itself may be virtualized.
    const auto canonicalRoot=std::filesystem::canonical(root);
    auto parent=root;
    for(const auto* child:{L"music",L"harmonia"}) {
        parent/=child;
        const auto attributes=GetFileAttributesW(parent.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES &&
            ((attributes&FILE_ATTRIBUTE_REPARSE_POINT) || !(attributes&FILE_ATTRIBUTE_DIRECTORY)))
            throw std::runtime_error("redirected/non-directory audio cache");
        std::filesystem::create_directory(parent);
    }
    const auto canonicalParent=std::filesystem::canonical(path.parent_path());
    const auto rel=canonicalParent.lexically_relative(canonicalRoot);
    if(rel.empty() || rel.is_absolute() || *rel.begin()=="..") throw std::runtime_error("cache directory escapes data root");
    if(std::filesystem::exists(path)) {
        if((GetFileAttributesW(path.c_str())&FILE_ATTRIBUTE_REPARSE_POINT) || ReadAudioFile(path)!=std::optional<AudioBytes>(bytes))
            throw std::runtime_error("existing audio cache differs");
        return relative;
    }
    const auto temporary=path.wstring()+L".tmp";
    HANDLE handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("audio cache temporary unavailable");
    DWORD written{};const bool ok=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size() && FlushFileBuffers(handle);
    CloseHandle(handle);
    if(!ok || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());throw std::runtime_error("audio cache write failed");
    }
    if(ReadAudioFile(path)!=std::optional<AudioBytes>(bytes)) throw std::runtime_error("audio cache verification failed");
    return relative;
}
}
