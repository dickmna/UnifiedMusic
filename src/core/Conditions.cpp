#include "Conditions.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace UnifiedMusic {
namespace {
std::uint32_t Word(const std::vector<std::uint8_t>& b,std::size_t p) {
    return b[p] | (std::uint32_t(b[p+1])<<8) | (std::uint32_t(b[p+2])<<16) | (std::uint32_t(b[p+3])<<24);
}
bool BooleanFunction(std::uint32_t fn) {return fn==56 || fn==59 || fn==67 || fn==310 || fn==359 || fn==707;}
ConditionTerm Canonical(ConditionTerm t) {
    if(BooleanFunction(t.function) && t.opcode==1) {
        if(t.comparison==0) {t.opcode=0;t.comparison=0x3f800000;}
        else if(t.comparison==0x3f800000) {t.opcode=0;t.comparison=0;}
    }
    return t;
}
}
std::optional<std::vector<ConditionTerm>> DecodeConditions(const TrackRecord& record,bool readOnly) {
    if(!record.singleAudio || record.deleted || record.invalid || record.conditions.size()>256) return {};
    const auto resolve=[&](std::uint32_t raw)->std::optional<FormKey> {
        const auto index=raw>>24;
        if(!raw || index>record.masters.size()) return {};
        return FormKey{index==record.masters.size()?record.source:record.masters[index],raw&0xffffff};
    };
    std::vector<ConditionTerm> result;
    for(const auto& b:record.conditions) {
        if(b.size()!=32 || (b[0]&0x1e) || (b[0]>>5)>5 || Word(b,28)!=0xffffffff) return {};
        ConditionTerm t;
        t.opcode=b[0]>>5;t.isOR=(b[0]&1)!=0;t.function=Word(b,8);t.comparison=Word(b,4);t.parameter2=Word(b,16);
        if(!std::isfinite(std::bit_cast<float>(t.comparison))) return {};
        if(t.function==18 || (readOnly && t.function==520)) {
            if(Word(b,12) || t.parameter2) return {};
        } else {
            if(t.function!=56 && t.function!=58 && t.function!=59 && t.function!=74 &&
               t.function!=67 && t.function!=310 && t.function!=359 && t.function!=707 &&
               !(readOnly && (t.function==117 || t.function==562))) return {};
            const auto parameter=resolve(Word(b,12));
            if(!parameter || (t.function!=59 && t.parameter2)) return {};
            t.parameter=*parameter;
        }
        if(Word(b,20)==2) {
            const auto ref=resolve(Word(b,24));
            if(!ref || *ref!=FormKey{"skyrim.esm",0x14}) return {};
            t.onPlayerReference=true;
        } else if(Word(b,20)!=0 || Word(b,24)) return {};
        result.push_back(t);
    }
    // An authored dangling OR has no successor. Appending without normalizing
    // it would change its meaning; keep that unsupported expression untouched.
    if(!readOnly && !result.empty() && result.back().isOR) return {};
    return result;
}
std::optional<Area> ConditionArea(const ConditionTerm& value,bool positive) {
    const auto t=Canonical(value);
    if(t.opcode || t.comparison!=(positive?0x3f800000u:0u)) return {};
    if(t.function==67) return Area{AreaKind::Cell,t.parameter};
    if(t.function==310) return Area{AreaKind::Worldspace,t.parameter};
    if(t.function==359) return Area{AreaKind::Location,t.parameter};
    return {};
}
bool OrdinaryConditions(std::span<const ConditionTerm> terms,bool battle) {
    if(terms.empty()) return true;
    if(battle) {
        if(terms.size()!=1 || terms.front().isOR) return false;
        const auto t=Canonical(terms.front());
        return t.function==707 && t.parameter==FormKey{"skyrim.esm",0x35d59} && t.opcode==0 && t.comparison==0;
    }
    // Time-of-day is a song's own eligibility, not a story/Boss ownership cue.
    return std::ranges::all_of(terms,[](const auto& t){return t.function==18;});
}
Clauses SplitClauses(std::span<const ConditionTerm> conditions) {
    Clauses result;
    std::size_t start=0;
    for(std::size_t i=0;i<conditions.size();++i) if(!conditions[i].isOR) {
        result.emplace_back(conditions.begin()+start,conditions.begin()+i+1);start=i+1;
    }
    return result;
}
bool SamePredicate(ConditionTerm a,ConditionTerm b) {
    a.isOR=b.isOR=false;return Canonical(a)==Canonical(b);
}
bool HasClause(std::span<const ConditionTerm> conditions,std::span<const ConditionTerm> clause) {
    std::size_t start=0;
    for(std::size_t i=0;i<conditions.size();++i) if(!conditions[i].isOR) {
        if(i+1-start==clause.size() && std::equal(conditions.begin()+start,conditions.begin()+i+1,clause.begin(),
            [](const auto& a,const auto& b){return a.isOR==b.isOR && SamePredicate(a,b);})) return true;
        start=i+1;
    }
    return false;
}
std::optional<Clauses> AddedClauses(std::span<const ConditionTerm> baseline,std::span<const ConditionTerm> authored) {
    if((!baseline.empty() && baseline.back().isOR) || (!authored.empty() && authored.back().isOR)) return {};
    for(const auto& clause:SplitClauses(baseline)) if(!HasClause(authored,clause)) return {};
    Clauses result;
    for(const auto& clause:SplitClauses(authored)) if(!HasClause(baseline,clause)) result.push_back(clause);
    return result;
}
std::vector<std::vector<ConditionTerm>> RepairClauses(const ConditionRepair& repair) {
    if(!repair.clauses.empty()) return repair.clauses;
    std::vector<std::vector<ConditionTerm>> result;
    for(const auto& area:repair.exclusions) {
        ConditionTerm term;term.function=area.kind==AreaKind::Cell?67:area.kind==AreaKind::Worldspace?310:359;
        term.parameter=area.form;result.push_back({term});
    }
    return result;
}
}
