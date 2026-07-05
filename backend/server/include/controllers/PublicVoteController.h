#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class PublicVoteController {
public:
    json getCandidates(const std::string& electionId);

    json checkVoter(const std::string& electionId, const std::string& voterId);

    json castVote(const std::string& electionId, const std::string& voterId,
                  const std::string& candidateName);

    json getResults(const std::string& electionId);
};
