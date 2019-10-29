#include "websocket/Handler.hpp"
#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "Redis.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
#include "jwt/json/json.hpp"
#include "websocket/Response.hpp"
#include "websocket/StateMachine.hpp"

namespace eventhub {
namespace websocket {

void Handler::process(ConnectionPtr conn, char* buf, size_t nBytes) {
  auto& fsm = conn->getWsFsm();
  fsm.process(buf, nBytes);

  switch (fsm.getState()) {
    case State::CONTROL_FRAME_READY:
      _handleControlFrame(conn);
      fsm.clearControlPayload();
      break;

    case State::DATA_FRAME_READY:
      _handleDataFrame(conn);
      fsm.clearPayload();
      break;
  }
}

void Handler::_handleDataFrame(ConnectionPtr conn) {
  auto& fsm     = conn->getWsFsm();
  auto& payload = fsm.getPayload();

  // Parse command and arguments.
  // Format is <COMMAND><SPACE><ARGUMENTS><NEWLINE>
  if (payload.length() > 1 && payload.substr(payload.length() - 1, payload.length()).compare("\n") == 0) {
    std::string command;
    std::string arg;
    auto argPos = payload.find_first_of(' ');

    if (argPos != std::string::npos) {
      command = payload.substr(0, argPos);
      arg     = payload.substr(argPos + 1, payload.length() - command.length() - 2);
    } else {
      command = payload.substr(0, payload.length() - 1);
    }

    _handleClientCommand(conn, command, arg);
  }
}

void Handler::_handleControlFrame(ConnectionPtr conn) {
  auto& fsm = conn->getWsFsm();

  switch (fsm.getControlFrameType()) {
    case response::Opcodes::CLOSE_FRAME:
      conn->shutdown();
      break;

    case response::Opcodes::PING_FRAME:
      response::sendData(conn, fsm.getControlPayload(), response::Opcodes::PONG_FRAME);
      break;

    case response::Opcodes::PONG_FRAME:
      // TODO: Calculate latency.
      break;
  }
}

void sendErrorMsg(ConnectionPtr conn, const std::string& errMsg, bool disconnect) {
  nlohmann::json j;
  j["error"] = errMsg;

  try {
    response::sendData(conn, j.dump(), response::Opcodes::TEXT_FRAME);
  } catch (...) {}

  if (disconnect) {
    response::sendData(conn, "", response::Opcodes::CLOSE_FRAME);
    conn->shutdown();
  }
}

void Handler::_handleClientCommand(ConnectionPtr conn, const std::string& command, const std::string& arg) {
  auto& accessController = conn->getAccessController();
  auto& redis            = conn->getWorker()->getServer()->getRedis();

  if (!accessController.isAuthenticated()) {
    LOG(ERROR) << "Disconnecting client in websocket mode with invalid authentication. This should never happen.";
    sendErrorMsg(conn, "Invalid authentication token.", true);
    return;
  }

  // Subscribe to a topic.
  if (command == "SUB") {
    if (!TopicManager::isValidTopicFilter(arg)) {
      sendErrorMsg(conn, arg + ": invalid topic.", false);
      return;
    }

    if (!accessController.allowSubscribe(arg)) {
      // TODO: Disconnect.
      sendErrorMsg(conn, "Insufficient access", false);
      return;
    }

    conn->subscribe(arg);
  }

  // Unsubscribe from a channel.
  else if (command == "UNSUB") {
    if (!TopicManager::isValidTopicFilter(arg)) {
      sendErrorMsg(conn, arg + ": invalid topic.", false);
      return;
    }

    conn->unsubscribe(arg);
  }

  // Unsubscribe from all channels.
  else if (command == "UNSUBALL") {
    conn->unsubscribeAll();
  }

  // Publish to a channel.
  else if (command == "PUB") {
    auto p = arg.find_first_of(' ');

    if (p == string::npos || p == arg.length() - 1) {
      sendErrorMsg(conn, "Payload cannot be blank", false);
      return;
    }

    auto topicName = arg.substr(0, p);
    auto payload   = arg.substr(p + 1, string::npos);

    if (!accessController.allowPublish(topicName)) {
      sendErrorMsg(conn, "Insufficient access to '" + topicName + "'", false);
      return;
    }

    if (!TopicManager::isValidTopicFilter(topicName)) {
      sendErrorMsg(conn, topicName + " is a invalid topic.", false);
      return;
    }

    try {
      auto cacheId = redis.cacheMessage(topicName, payload);
      if (cacheId.length() == 0) {
        LOG(ERROR) << "Failed to cache message to Redis.";
        sendErrorMsg(conn, "Failed to cache message to Redis. Discarding.", false);
        return;
      }

      redis.publishMessage(topicName, cacheId, payload);
    } catch (std::exception& e) {
      LOG(ERROR) << "Redis error while publishing message: " << e.what();
      sendErrorMsg(conn, "Redis error while publishing message.", false);
      return;
    }
  }

  // List all subscribed channels.
  else if (command == "LIST") {
    nlohmann::json j = nlohmann::json::array();

    for (auto subscription : conn->listSubscriptions()) {
      j.push_back(subscription);
    }

    websocket::response::sendData(conn, j.dump(), websocket::response::TEXT_FRAME);
  } else if (command == "QUIT") {
    response::sendData(conn, "", response::Opcodes::CLOSE_FRAME);
    conn->shutdown();
  } else {
    sendErrorMsg(conn, "Unknown command", false);
  }
}

} // namespace websocket
} // namespace eventhub
