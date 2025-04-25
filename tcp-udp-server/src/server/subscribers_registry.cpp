#include "subscribers_registry.hpp"
#include <stdexcept>

auto SubscribersRegistry::get_subscriber_by_sockfd(int sockfd)
    -> std::shared_ptr<SubscriberInfo> {
  auto it = sock_subscribers_.find(sockfd);
  if (it == sock_subscribers_.end()) {
    throw std::runtime_error("Subscriber not connected");
  }
  return it->second;
}

void SubscribersRegistry::connect_subscriber(int sockfd,
                                             const std::string &id) {
  // If the subscriber already exists and is not connected, mark it as connected
  // If the subscriber already exists and is connected, throw an error
  auto it = id_subscribers_.find(id);
  if (it != id_subscribers_.end()) {
    if (it->second->is_connected()) {
      throw std::runtime_error("Subscriber already connected");
    }
    it->second->sockfd = sockfd;
    sock_subscribers_[sockfd] = it->second;
  } else {
    // If the subscriber does not exist, create a new one
    auto subscriber = std::make_shared<SubscriberInfo>(id, sockfd);
    sock_subscribers_[sockfd] = subscriber;
    id_subscribers_[id] = subscriber;
  }
}

void SubscribersRegistry::disconnect_subscriber(int sockfd) {
  auto it = sock_subscribers_.find(sockfd);

  if (it == sock_subscribers_.end()) {
    return;
  }

  auto subscriber = it->second;
  subscriber->sockfd = -1;
  sock_subscribers_.erase(it);
}

auto SubscribersRegistry::get_subscriber_id(int sockfd) -> const std::string & {
  auto subscriber = get_subscriber_by_sockfd(sockfd);
  return subscriber->id;
}

void SubscribersRegistry::subscribe_to_topic(int sockfd, TokenPattern topic) {

  auto subscriber = get_subscriber_by_sockfd(sockfd);

  subscriber->topics.insert(topic);
  topic_subscribers_[topic].insert(subscriber);
}

void SubscribersRegistry::unsubscribe_from_topic(int sockfd,
                                                 TokenPattern topic) {
  auto subscriber = get_subscriber_by_sockfd(sockfd);
  subscriber->topics.erase(topic);

  auto it = topic_subscribers_.find(topic);
  if (it != topic_subscribers_.end()) {
    auto &subscribers = it->second;
    if (subscribers.size() == 1) {
      topic_subscribers_.erase(it);
    } else {
      subscribers.erase(subscriber);
    }
  }
}

auto SubscribersRegistry::retrieve_topic_subscribers(const TokenPattern &topic)
    -> std::unordered_set<int> {
  std::unordered_set<int> subscribers_sockets{};

  // Search for subscriber topics that match the given topic
  for (const auto &[subscriber_topic, subscribers] : topic_subscribers_) {
    if (subscriber_topic.matches(topic)) {
      for (const auto &subscriber : subscribers) {
        if (subscriber->is_connected()) {
          subscribers_sockets.insert(subscriber->sockfd);
        }
      }
    }
  }

  return subscribers_sockets;
}
