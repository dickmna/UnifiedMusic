#pragma once
#include "PluginScan.h"
#include <span>

namespace UnifiedMusic {
// Read-only witnesses retain a terminal OR exactly; it may not be appended to.
std::optional<std::vector<ConditionTerm>> DecodeConditions(const TrackRecord& record, bool readOnly = false);
std::optional<Area> ConditionArea(const ConditionTerm& term, bool positive);
bool OrdinaryConditions(std::span<const ConditionTerm> terms, bool battle);
using Clauses = std::vector<std::vector<ConditionTerm>>;
Clauses SplitClauses(std::span<const ConditionTerm> conditions);
std::optional<Clauses> AddedClauses(std::span<const ConditionTerm> baseline, std::span<const ConditionTerm> authored);
bool SamePredicate(ConditionTerm a, ConditionTerm b);
bool HasClause(std::span<const ConditionTerm> conditions, std::span<const ConditionTerm> clause);
std::vector<std::vector<ConditionTerm>> RepairClauses(const ConditionRepair& repair);
}
