// -*-c++-*---------------------------------------------------------------------------------------
// Copyright 2024 Bernd Pfrommer <bernd.pfrommer@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FLEX_SYNC__SYNC_UTILS_HPP_
#define FLEX_SYNC__SYNC_UTILS_HPP_

#include <boost/shared_ptr.hpp>
#include <map>
#include <rclcpp/rclcpp.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace flex_sync
{
// helper function to make vector from elements in queue,
// and clear out the queue.
template <typename T>
static std::vector<std::shared_ptr<T const>> make_vec(
  const rclcpp::Time & t, const std::vector<std::string> & topics,
  std::map<std::string, std::map<rclcpp::Time, std::shared_ptr<T const>>> *
    topicToQueue)
{
  std::vector<std::shared_ptr<T const>> mvec;
  for (const auto & topic : topics) {
    auto & t2m = (*topicToQueue)[topic];  // time to message
    if (t2m.empty()) {
      throw std::runtime_error(topic + " has empty queue!");
    }
    while (!t2m.empty() && t2m.begin()->first < t) {
      t2m.erase(t2m.begin());
    }
    if (t2m.empty()) {
      RCLCPP_ERROR_STREAM(
        rclcpp::get_logger("sync"), topic << " queue is empty for time: " << t);
      throw std::runtime_error(topic + " queue is empty!");
    }
    if (t2m.begin()->first != t) {
      throw std::runtime_error(topic + " has wrong time stamp!");
    }
    mvec.push_back(t2m.begin()->second);
    t2m.erase(t2m.begin());
  }
  return (mvec);
}

// helper function to update the message count in map
static inline std::map<rclcpp::Time, int>::iterator update_count(
  const rclcpp::Time & t, std::map<rclcpp::Time, int> * msgCountMap)
{
  // check if we have this time stamp
  std::map<rclcpp::Time, int>::iterator it = msgCountMap->find(t);
  if (it == msgCountMap->end()) {  // no messages received for this tstamp
    msgCountMap->insert(std::map<rclcpp::Time, int>::value_type(t, 1));
    it = msgCountMap->find(t);
  } else {
    it->second++;  // bump number of received messages
  }
  return (it);
}

// helper function to drop message
static inline void decrease_count(
  const rclcpp::Time & t, std::map<rclcpp::Time, int> * msgCountMap)
{
  // check if we have this time stamp
  std::map<rclcpp::Time, int>::iterator it = msgCountMap->find(t);
  if (it == msgCountMap->end()) {
    // no messages received for this tstamp, should not happen!
    RCLCPP_ERROR_STREAM(rclcpp::get_logger("sync"), "no count for " << t);
    return;
  } else {
    it->second--;
    if (it->second <= 0) {
      msgCountMap->erase(it);
    }
  }
}

}  // namespace flex_sync

#endif  // FLEX_SYNC__SYNC_UTILS_HPP_
