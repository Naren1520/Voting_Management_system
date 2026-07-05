#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class ElectionController {
public:
    json getElections(const std::string& userId);

    json createElection(const std::string& userId,
                        const std::string& title,
                        const std::string& electionType = "standard",
                        const std::string& scheduleType = "always_on",
                        const std::string& startsAt     = "",
                        const std::string& endsAt       = "",
                        const std::string& scheduleJson = "",
                        const std::string& timezone     = "UTC");

    json getElection(const std::string& userId, const std::string& electionId);

    json deleteElection(const std::string& userId, const std::string& electionId);

    json updateSchedule(const std::string& userId, const std::string& electionId,
                        const json& body);
};
