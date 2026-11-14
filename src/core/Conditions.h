#pragma once
#include "PluginScan.h"
#include <span>

namespace UnifiedMusic {
std::optional<std::vector<ConditionTerm>> DecodeConditions(const TrackRecord& record);
std::optional<Area> ConditionArea(const ConditionTerm& term, bool positive);
bool OrdinaryConditions(std::span<const ConditionTerm> terms, bool battle);
std::vector<ConditionTerm> NegateScope(std::span<const ConditionTerm> scope);
bool SamePredicate(ConditionTerm a, ConditionTerm b);
bool HasClause(std::span<const ConditionTerm> conditions, std::span<const ConditionTerm> clause);
std::vector<std::vector<ConditionTerm>> RepairClauses(const ConditionRepair& repair);
}
