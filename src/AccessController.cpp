#include "AccessController.hpp"
#include "TopicManager.hpp"
#include <stdexcept>
#include <string>
#include <vector>

/*
{
  "exp": 1300819380,
  "sub": "ole.skudsvik@gmail.com",
  "read": [ "channel1", "channel2" ],
  "write": [ "channel1", "channel2" ],
  "createtoken": [ ".*" ]
}
 */

namespace eventhub {

#define REQUIRE_TOKEN_LOADED(x) \
  if (!_token_loaded)           \
    return false;

AccessController::AccessController() {
  _token_loaded = false;
}

AccessController::~AccessController() {
}

// authenticate loads a a JWT token and extracts ACL's for publish and subscribe.
bool AccessController::authenticate(const std::string& jwtToken, const std::string& secret) {
  try {
    _token = jwt::decode(jwtToken, jwt::params::algorithms({"hs256"}), jwt::params::secret(secret));

    auto& payload = _token.payload();

    if (payload.has_claim("write")) {
      for (auto filter : payload.get_claim_value<std::vector<std::string>>("write")) {
        if (TopicManager::isValidTopicFilter(filter)) {
          _publish_acl.push_back(filter);
        }
      }
    }

    if (payload.has_claim("read")) {
      for (auto filter : payload.get_claim_value<std::vector<std::string>>("read")) {
        if (TopicManager::isValidTopicFilter(filter)) {
          _subscribe_acl.push_back(filter);
        }
      }
    }

    if ((_subscribe_acl.size() + _publish_acl.size()) == 0) {
      throw std::invalid_argument("No publish or subscribe ACL defined in JWT token.");
    }
  } catch (std::exception& e) {
    LOG(ERROR) << "Error in AccessController: " << e.what();
    return false;
  }

  _token_loaded = true;

  return true;
}

bool AccessController::isAuthenticated() {
  REQUIRE_TOKEN_LOADED();
  return true;
}

// allowPublish checks if the loaded token is allowed to publish to topic.
bool AccessController::allowPublish(const std::string& topic) {
  REQUIRE_TOKEN_LOADED();

  for (auto& filter : _publish_acl) {
    if (TopicManager::isFilterMatched(filter, topic)) {
      return true;
    }
  }

  return false;
}

// allowPublish checks if the loaded token is allowed to subscribe to topic.
bool AccessController::allowSubscribe(const std::string& topic) {
  REQUIRE_TOKEN_LOADED();

  for (auto& filter : _subscribe_acl) {
    if (TopicManager::isFilterMatched(filter, topic)) {
      return true;
    }
  }

  return false;
}

bool AccessController::allowCreateToken(const std::string& path) {
  REQUIRE_TOKEN_LOADED();
  // Not implemented yet.
  return true;
}

} // namespace eventhub