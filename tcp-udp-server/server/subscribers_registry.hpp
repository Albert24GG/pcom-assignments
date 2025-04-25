#pragma once

#include "token_pattern.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

class SubscribersRegistry {
  struct SubscriberInfo {
    explicit SubscriberInfo(std::string id, int sockfd)
        : id(std::move(id)), sockfd(sockfd) {}

    bool is_connected() const { return sockfd > 0; }

    std::string id{};
    std::unordered_set<TokenPattern> topics{};
    int sockfd{-1};
  };

public:
  /**
   * @brief Handle a new subscriber connection
   *
   * @param sockfd The socket file descriptor of the subscriber
   * @param id The id of the subscriber
   *
   * @throws std::runtime_error if the subscriber is already connected
   */
  void connect_subscriber(int sockfd, const std::string &id);

  /**
   * @brief Handle a subscriber disconnection
   * If the subscriber is not connected, this function does nothing
   *
   * @param sockfd The socket file descriptor of the subscriber
   */
  void disconnect_subscriber(int sockfd);

  /**
   * @brief Check if a subscriber is connected
   *
   * @param sockfd The socket file descriptor of the subscriber
   * @return true if the subscriber is connected
   */
  bool is_subscriber_connected(int sockfd) {
    return sock_subscribers_.find(sockfd) != sock_subscribers_.end();
  }

  /**
   * @brief Retrieve the id of a subscriber
   *
   * @param sockfd The socket file descriptor of the subscriber
   * @return The id of the subscriber
   *
   * @throws std::runtime_error if there is no subscriber connected on the given
   * socket
   */
  auto get_subscriber_id(int sockfd) -> const std::string &;

  /**
   * @brief Subscribe a subscriber to a topic
   *
   * @param sockfd The socket file descriptor of the subscriber
   * @param topic The topic to subscribe to
   *
   * @throws std::runtime_error if there is no subscriber connected on the given
   * socket
   */
  void subscribe_to_topic(int sockfd, TokenPattern topic);

  /**
   * @brief Unsubscribe a subscriber from a topic
   *
   * @param sockfd The socket file descriptor of the subscriber
   * @param topic The topic to unsubscribe from
   *
   * @throws std::runtime_error if there is no subscriber connected on the given
   * socket
   */
  void unsubscribe_from_topic(int sockfd, TokenPattern topic);

  /**
   * @brief Retrieve the socket file descriptors of subscribers subscribed to a
   * topic
   *
   * @param topic The topic to retrieve subscribers for
   * @return A set of socket file descriptors of subscribers subscribed to the
   * topic
   */
  auto retrieve_topic_subscribers(const TokenPattern &topic)
      -> std::unordered_set<int>;

private:
  auto get_subscriber_by_sockfd(int sockfd) -> std::shared_ptr<SubscriberInfo>;

  // mapping of socket file descriptors to subscriber's info
  std::unordered_map<int, std::shared_ptr<SubscriberInfo>> sock_subscribers_;
  // mapping of subscriber's id to subscriber's info
  std::unordered_map<std::string, std::shared_ptr<SubscriberInfo>>
      id_subscribers_;

  // mapping of topic to subscriber's info
  std::unordered_map<TokenPattern,
                     std::unordered_set<std::shared_ptr<SubscriberInfo>>>
      topic_subscribers_;
};
