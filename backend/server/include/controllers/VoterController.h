#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class VoterController {
public:
    bool ownsElection(const std::string& userId, const std::string& electionId);

    json getVoters(const std::string& userId, const std::string& electionId);

    json addVoter(const std::string& userId, const std::string& electionId,
                  const std::string& voterId, const std::string& name,
                  const std::string& email, const std::string& phone);

    json syncVoters(const std::string& userId, const std::string& electionId,
                    const json& voterList);

    json deleteVoter(const std::string& userId, const std::string& electionId,
                     const std::string& voterId);
};
