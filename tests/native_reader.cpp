#include "NativePluginInput.h"
#include "core/PluginScan.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using namespace UnifiedMusic;
namespace {
unsigned checks=0;
void Check(bool value,const char* label) {
    ++checks;
    if(!value) {std::fprintf(stderr,"native reader: %s\n",label);std::exit(1);}
}
std::uint32_t Code(std::string_view tag) {
    return std::uint8_t(tag[0])|(std::uint32_t(std::uint8_t(tag[1]))<<8)|
        (std::uint32_t(std::uint8_t(tag[2]))<<16)|(std::uint32_t(std::uint8_t(tag[3]))<<24);
}
using Bytes=std::vector<std::uint8_t>;
Bytes Text(std::string value) {Bytes bytes(value.begin(),value.end());bytes.push_back(0);return bytes;}
Bytes Word(std::uint32_t value) {return {std::uint8_t(value),std::uint8_t(value>>8),std::uint8_t(value>>16),std::uint8_t(value>>24)};}
struct Field {std::string tag;Bytes bytes;bool extended{};};
struct Row {
    std::string tag;
    std::uint32_t id{},flags{};
    std::vector<Field> fields;
    std::uint32_t offset{},stored{},expanded{};
};
// This is a scripted TESFile contract double, not a game emulator. Payloads
// are already decoded; no claim of executing native decompression is made.
struct File {
    struct Header {std::uint32_t form{},length{},flags{},formID{};} currentform;
    std::string name="Addon.esp";
    std::uint8_t compile=3;
    bool light{};
    std::uint16_t small=0x123;
    File** masterPtrs{};
    std::uint32_t masterCount{},filesize{},fileOffset{},formoffset{},actualChunkSize{},decompressedFormBufferSize{};
    void* lockedFile{};
    void* decompressedFormBuffer{};
    std::vector<Row> rows;
    std::size_t row{},field{};
    bool chunk{},openOK=true,closeOK=true,readOK=true,earlyForm=false,skipForm=false,earlySub=false,badChunk=false;
    bool oversizedChunk=false,missingDecompression=false,oversizedDecompression=false,closeThrows=false;
    unsigned opens{},closes{},reads{};
    std::vector<std::string> readFields;
    std::string_view GetFilename() const {return name;}
    bool IsLight() const {return light;}
    unsigned GetCompileIndex() const {return compile;}
    unsigned GetSmallFileCompileIndex() const {return small;}
    void Layout() {
        std::uint32_t position=0;
        for(auto& item:rows) {
            item.offset=position;
            item.expanded=0;
            for(const auto& f:item.fields) item.expanded+=6+static_cast<unsigned>(f.bytes.size())+(f.extended?10:0);
            if(item.tag!="GRUP") item.stored=(item.flags&0x40000)?17:item.expanded;
            position+=item.tag=="GRUP"?24:24+item.stored;
        }
        filesize=position;
    }
    void Select() {
        field=0;chunk=false;formoffset=actualChunkSize=decompressedFormBufferSize=0;decompressedFormBuffer=nullptr;
        if(row==rows.size()) {currentform={};fileOffset=filesize;return;}
        const auto& item=rows[row];
        currentform={Code(item.tag),item.stored,item.flags,item.id};fileOffset=item.offset;
    }
    bool OpenRead() {++opens;row=0;Select();return openOK;}
    bool CloseTES(bool force) {Check(!force,"close is non-forcing");++closes;if(closeThrows) throw std::runtime_error("close exception");return closeOK;}
    std::uint32_t GetCurrentSubRecordType() {
        auto& item=rows[row];
        if(currentform.flags&0x40000) {
            decompressedFormBuffer=missingDecompression?nullptr:this;
            decompressedFormBufferSize=oversizedDecompression?128*1024*1024+1:item.expanded;
        }
        if(field==item.fields.size()) return 0;
        const auto& f=item.fields[field];
        if(!chunk) {if(f.extended)formoffset+=10;chunk=true;}
        actualChunkSize=oversizedChunk?std::numeric_limits<unsigned>::max():static_cast<unsigned>(f.bytes.size());
        return badChunk?0:Code(f.tag);
    }
    bool ReadData(void* target,std::uint32_t size) {
        ++reads;readFields.push_back(rows[row].fields[field].tag);
        if(!readOK)return false;
        Check(size==rows[row].fields[field].bytes.size(),"whole requested payload, no truncation");
        std::memcpy(target,rows[row].fields[field].bytes.data(),size);return true;
    }
    bool SeekNextSubrecord() {
        formoffset+=6+actualChunkSize;++field;chunk=false;
        if(earlySub)return false;
        if(field==rows[row].fields.size()) {actualChunkSize=0;return false;}
        GetCurrentSubRecordType();return true;
    }
    bool Next(bool skipIgnored,bool skipInit) {
        Check(!skipIgnored && skipInit,"all source versions; lazy subrecord initialization");
        if(earlyForm)return false;
        row+=skipForm?2:1;
        if(row>rows.size())row=rows.size();
        Select();return row<rows.size();
    }
};
void Visit(File& file,const RecordConsumer& consume) {
    NativeInput::Read(file,[](auto& f){return f.OpenRead();},
        [](auto& f){return f.Next(false,true);},consume);
}
void Read(PluginScan& scan,File& file) {scan.Read(file.name,[&](const auto& consume){Visit(file,consume);});}
File Fixture(File** masters) {
    File file;file.masterPtrs=masters;file.masterCount=1;
    file.rows={
        {"TES4",0,0,{{"HEDR",Bytes(12)},{"MAST",Text("Skyrim.esm")}}},
        {"GRUP"},
        {"MUSC",0x100,0,{{"EDID",Text("MUSExploreForest")},{"FNAM",Word(4)},
            {"TNAM",{0,2,0,0,0,8,0,1}},{"UNKN",{}}}},
        {"MUST",0x03000800,0x40000,{{"EDID",Text(std::string(70000,'A')),true},
            {"CNAM",Word(0x6ed7e048)},{"ANAM",Text("Music/New.xwm")},
            {"CTDA",Bytes(32)},{"CITC",Word(1)},{"SNAM",Word(0x200)}}},
        {"REFR",0x03000900,0,{{"EDID",Text("NotUsed")},{"VMAD",{5,0,2,0,0,0}},{"DATA",Bytes(12)}}},
        {"LCTN",0x03000A00,0,{{"EDID",Text("Region")},{"NAM1",Word(0x100)},{"PNAM",Word(0x01000B00)}}},
        {"REGN",0x03000C00,0,{{"EDID",Text("Area")},{"RDAT",Word(7)},{"RDMO",Word(0x100)}}},
        {"MUSC",0x200,0x20,{{"EDID",Text("Deleted")}}},
        {"MUSC",0xFF000800,0x1000,{{"EDID",Text("Ignored")}}},
        {"GRUP"}
    };
    file.Layout();file.rows[1].stored=file.filesize-file.rows[1].offset;
    file.rows.back().stored=24;
    return file;
}
}
int main() {
    File base;base.name="Skyrim.esm";base.compile=0;
    File light;light.name="Light.esl";light.light=true;light.small=0xABC;
    File other;other.name="Other.esm";other.compile=7;
    File* masters[]{&base};
    {
        auto file=Fixture(masters);PluginScan scan;Read(scan,file);
        Check(file.opens==1 && file.closes==1,"one open/close");
        const auto& music=scan.Records().at({"skyrim.esm",0x100}).front();
        Check(music.tracks==std::vector<FormKey>{{"skyrim.esm",0x200},{"addon.esp",0x800}},"subrecord IDs stay file-local");
        const auto& track=scan.Tracks().at({"addon.esp",0x800}).front();
        Check(track.audioPath=="Music/New.xwm" && track.conditions==std::vector<Bytes>{Bytes(32)},"audio and CTDA preserved");
        Check(track.metadata.find(std::string(70000,'A'))!=std::string::npos,"extended field not truncated");
        Check(scan.Records().at({"skyrim.esm",0x200}).front().deleted,"deleted history retained");
        Check(scan.Records().size()==2,"ignored invalid IDs do not create records");
        Check(std::ranges::count(file.readFields,"VMAD")==1,"scripts outside music types are read");
        Check(std::ranges::count(file.readFields,"PNAM")==1 && std::ranges::count(file.readFields,"RDMO")==1,"area and ancestry fields read");
        Check(std::ranges::count(file.readFields,"DATA")==0 && std::ranges::count(file.readFields,"RDAT")==0,"unneeded data not copied");
    }
    {
        File* mixed[]{&other,&light,&base};File f;f.masterPtrs=mixed;f.masterCount=3;
        Check(NativeInput::SourceID(f,0x07000100)==0x100,"full master order independent of load index");
        Check(NativeInput::SourceID(f,0xFEABC812)==0x01000812,"light master header normalization");
        Check(NativeInput::SourceID(f,0x200)==0x02000200,"base master may be last");
        Check(NativeInput::SourceID(f,0x03000800)==0x03000800,"own full header normalization");
        f.light=true;
        Check(NativeInput::SourceID(f,0xFE123800)==0x03000800,"own light header normalization");
        try {NativeInput::SourceID(f,0x09000800);Check(false,"unresolved header accepted");}catch(const std::runtime_error&){++checks;}
    }
    const auto reject=[&](const char* label,auto change,const char* reason,unsigned opened=1,unsigned closed=1) {
        auto file=Fixture(masters);change(file);PluginScan scan;bool caught=false;
        try {Read(scan,file);}catch(const std::runtime_error& e) {caught=std::string(e.what()).find(reason)!=std::string::npos;}
        Check(caught,label);Check(scan.Records().empty() && scan.Tracks().empty(),"failure admits no partial plugin");
        Check(file.opens==opened && file.closes==closed,"failure closes only owned session");
    };
    reject("open failure",[](auto& f){f.openOK=false;},"open failed",1,0);
    reject("locked writer",[](auto& f){f.lockedFile=&f;},"locked writer",0,0);
    reject("short payload",[](auto& f){f.readOK=false;},"read failed");
    reject("early form EOF",[](auto& f){f.earlyForm=true;},"ended early");
    reject("skipped form",[](auto& f){f.skipForm=true;},"skipped data");
    reject("early field EOF",[](auto& f){f.earlySub=true;},"ended early");
    reject("zero tag",[](auto& f){f.badChunk=true;},"subrecord boundary");
    reject("oversized field",[](auto& f){f.oversizedChunk=true;},"subrecord boundary");
    reject("decompression failure",[](auto& f){f.missingDecompression=true;},"decompression unavailable");
    reject("decompression limit",[](auto& f){f.oversizedDecompression=true;},"decompression unavailable");
    reject("invalid group",[](auto& f){f.rows[1].stored=f.filesize+1;},"group boundary");
    reject("short record",[](auto& f){f.rows[2].stored=f.filesize;},"record size");
    reject("no header",[](auto& f){f.rows[0].tag="JUNK";},"TES4 header");
    reject("changed masters",[](auto& f){f.rows[0].fields[1].bytes=Text("Missing.esm");f.Layout();},"master table differs");
    reject("missing master table",[](auto& f){f.masterPtrs=nullptr;},"master table differs");
    reject("short master table",[](auto& f){f.masterCount=2;},"incomplete native master");
    reject("unresolved record header",[](auto& f){f.rows[3].id=0x09000800;},"no source master");
    reject("unresolved payload",[](auto& f){f.rows[2].fields[2].bytes={0,8,0,4,0,8,0,1};},"invalid music form reference");
    reject("close failure",[](auto& f){f.closeOK=false;},"close failed");
    reject("close exception",[](auto& f){f.closeThrows=true;},"close exception");
    {
        auto file=Fixture(masters);bool caught=false;
        try {Visit(file,[](const auto&){throw std::runtime_error("consumer failed");});}
        catch(const std::runtime_error&){caught=true;}
        Check(caught && file.closes==1,"consumer exception closes once");
    }
    {
        File empty;empty.rows={{"TES4"}};empty.Layout();PluginScan scan;Read(scan,empty);
        Check(scan.Records().empty() && empty.closes==1,"empty header fields");
    }
    std::printf("Native reader contract: %u checks passed (engine API double; no game execution)\n",checks);
}
