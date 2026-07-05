#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class CandidateController {
public:
    bool ownsElection(const std::string& userId, const std::string& electionId);

    json getCandidates(const std::string& userId, const std::string& electionId);

    json addCandidate(const std::string& userId, const std::string& electionId,
                      const std::string& name);

    json deleteCandidate(const std::string& userId, const std::string& electionId,
                         const std::string& name);
};
