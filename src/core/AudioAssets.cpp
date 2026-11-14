#include "AudioAssets.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <json/json.h>
#include <stdexcept>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>

namespace UnifiedMusic {
std::map<FormKey,std::string> CheckSceneResources(const std::vector<SceneResource>& resources,const AudioReader& reader) {
    std::map<FormKey,std::string> failures;
    const auto valid=[&](const std::string& path) {
        const auto normalized=MusicPath(path);
        const auto bytes=normalized?reader(*normalized):std::nullopt;
        return bytes && ValidMusic(*bytes);
    };
    for(const auto& resource:resources)
        if(!valid(resource.audioPath) || (!resource.finalePath.empty() && !valid(resource.finalePath)))
            failures.try_emplace(resource.musicType,"missing/invalid scene resource for "+ToString(resource.track));
    return failures;
}
namespace {
std::uint32_t U32(std::span<const std::uint8_t> b,std::size_t p) {
    if(p>b.size() || b.size()-p<4) throw std::runtime_error("truncated audio integer");
    return b[p]|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);
}
AudioBytes ReadAt(std::ifstream& f,std::uint64_t offset,std::size_t count,std::uint64_t length) {
    if(offset>length || count>length-offset || count>MaxAudioBytes) throw std::runtime_error("audio file bounds exceeded");
    AudioBytes b(count); f.clear(); f.seekg(static_cast<std::streamoff>(offset));
    if(count && !f.read(reinterpret_cast<char*>(b.data()),count)) throw std::runtime_error("short audio file read");
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

void OriginalAudioPack::Read(const std::filesystem::path& manifest) {
    assets_.clear();pairs_.clear();fingerprints_.clear();
    if(!std::filesystem::exists(manifest)) return;
    if(std::filesystem::file_size(manifest)>1024*1024) throw std::runtime_error("audio manifest too large");
    std::ifstream stream(manifest);Json::Value root;Json::CharReaderBuilder builder;
    builder["rejectDupKeys"]=true;builder["failIfExtra"]=true;
    std::string errors;
    if(!Json::parseFromStream(builder,stream,&root,&errors) || !root.isObject() || root["format"]!=1 ||
       !root["assets"].isObject() || !root["pairs"].isArray() || root["assets"].empty() ||
       root["assets"].size()>1024 || root["pairs"].empty() || root["pairs"].size()>2048)
        throw std::runtime_error("invalid audio manifest");
    const auto digest=[](const std::string& hash) {
        return hash.size()==64 && std::ranges::all_of(hash,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
    };
    std::map<std::string,Asset> assets;
    std::map<std::string,std::string> files;
    for(const auto& source:root["assets"].getMemberNames()) {
        const auto& value=root["assets"][source];
        if(MusicPath(source)!=source || source.starts_with("music/harmonia/") || !value.isObject() ||
           !value["file"].isString() || !value["sha256"].isString() || !value["equivalent_sha256"].isArray() ||
           value["equivalent_sha256"].empty() || value["equivalent_sha256"].size()>16)
            throw std::runtime_error("invalid audio asset mapping");
        Asset asset{value["file"].asString(),value["sha256"].asString(),{}};
        if(MusicPath(asset.file)!=asset.file || !asset.file.starts_with("music/harmonia/original/") || !digest(asset.hash))
            throw std::runtime_error("invalid optional audio path/hash");
        asset.equivalent.insert(asset.hash);
        for(const auto& hash:value["equivalent_sha256"]) {
            if(!hash.isString() || !digest(hash.asString())) throw std::runtime_error("invalid original audio fingerprint");
            asset.equivalent.insert(hash.asString());
        }
        if(auto it=files.find(asset.file);it!=files.end() && it->second!=asset.hash)
            throw std::runtime_error("conflicting optional audio asset");
        files[asset.file]=asset.hash;assets.emplace(source,std::move(asset));
    }
    std::set<AudioPaths> pairs;
    for(const auto& pair:root["pairs"]) {
        if(!pair.isArray() || pair.size()!=2 || !pair[0].isString() || !pair[1].isString())
            throw std::runtime_error("invalid main/finale pair");
        AudioPaths paths{pair[0].asString(),pair[1].asString()};
        if(!assets.contains(paths.first) || (!paths.second.empty()&&!assets.contains(paths.second)) || !pairs.insert(paths).second)
            throw std::runtime_error("unknown/duplicate main/finale pair");
    }
    assets_=std::move(assets);pairs_=std::move(pairs);
}
std::string OriginalAudioPack::Fingerprint(const std::string& path,const AudioReader& reader) {
    const auto normalized=MusicPath(path);if(!normalized) return {};
    if(auto it=fingerprints_.find(*normalized);it!=fingerprints_.end()) return it->second;
    const auto bytes=reader(*normalized);
    auto hash=bytes && ValidMusic(*bytes)?AudioHash(*bytes):std::string{};
    fingerprints_.emplace(*normalized,hash);return hash;
}
std::optional<AudioPair> OriginalAudioPack::Resolve(const AudioPaths& source,const std::vector<AudioPaths>& present,
    const AudioReader& reader,std::string& reason) {
    if(Empty()) {reason="optional audio pack not installed";return {};}
    const auto main=MusicPath(source.first),end=source.second.empty()?std::optional<std::string>(""):MusicPath(source.second);
    if(!main || !end) {reason="unsupported music path";return {};}
    if(!pairs_.contains({*main,*end})) {reason="no verified main/finale mapping";return {};}
    const auto& asset=assets_.at(*main);
    const auto* finale=end->empty()?nullptr:&assets_.at(*end);
    if(Fingerprint(asset.file,reader)!=asset.hash || (finale && Fingerprint(finale->file,reader)!=finale->hash)) {
        reason="missing/changed optional main/finale resource";return {};
    }
    AudioPair pair{asset.file,finale?finale->file:"",asset.hash,finale?finale->hash:"",
        Fingerprint(*main,reader),finale?Fingerprint(*end,reader):""};
    if(pair.replacementHash.empty() || (finale && pair.replacementFinaleHash.empty())) {
        reason="missing/invalid winning main/finale resource";return {};
    }
    const auto equivalent=[&](const AudioPaths& paths) {
        return asset.equivalent.contains(Fingerprint(paths.first,reader)) &&
            (finale?(!paths.second.empty() && finale->equivalent.contains(Fingerprint(paths.second,reader))):paths.second.empty());
    };
    if(equivalent(source) || std::ranges::any_of(present,equivalent)) {
        reason="original already present";return {};
    }
    reason="verified optional original alongside replacement";return pair;
}
}
