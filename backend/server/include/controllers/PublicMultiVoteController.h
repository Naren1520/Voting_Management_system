#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class PublicMultiVoteController {
public:
    json getBallot(const std::string& electionId);

    json checkVoter(const std::string& electionId, const std::string& voterId);

    json castVotes(const std::string& electionId, const std::string& voterId,
                   const json& votes);

    json getResults(const std::string& electionId);
};
