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

#pragma once

#include "flex_sync/sync.h"

/*
 * Class for synchronized subscriber
 */

namespace flex_sync
{

// -------------- topic class, used by all subscriber syncs  ------------

template <class T, class S>
class Topic
{
public:
  typedef std::shared_ptr<T const> TConstPtr;
  Topic(
    const std::string & topic, ros::NodeHandle & nh, unsigned int qs, S * sync)
  : topic_(topic), sync_(sync)
  {
    sub_ = nh.subscribe(topic, qs, &Topic::callback, this);
  }
  void callback(TConstPtr const & msg) { sync_->process(topic_, msg); }

private:
  std::string topic_;
  S * sync_;
  ros::Subscriber sub_;
};
template <typename... Ts>
class SubscribingSync
{
};

// -------------- subscriber sync for two different type of topics ------

template <class T1, class T2>
class SubscribingSync<T1, T2>
{
public:
  using string = std::string;
  using Time = rclcpp::Time;
  typedef Sync<T1, T2> Sync2;
  typedef typename Sync<T1, T2>::Callback Callback;

  SubscribingSync(
    const ros::NodeHandle & nh, const std::vector<std::vector<string>> & topics,
    const Callback & callback, unsigned int maxQueueSize = 5)
  : sync_(
      std::vector<std::vector<string>>(2, std::vector<string>()), callback,
      maxQueueSize),
    nh_(nh)
  {
    if (topics.size() > 0) {
      for (const auto & topic : topics[0]) {
        addTopic1(topic);
      }
    }
    if (topics.size() > 1) {
      for (const auto & topic : topics[1]) {
        addTopic2(topic);
      }
    }
  }
  void addTopic1(const std::string & t)
  {
    topics1_.emplace_back(new Topic<T1, Sync2>(t, nh_, sync_.qs(), &sync_));
    sync_.addTopic1(t);
  }
  void addTopic2(const std::string & t)
  {
    topics2_.emplace_back(new Topic<T2, Sync2>(t, nh_, sync_.qs(), &sync_));
    sync_.addTopic2(t);
  }

private:
  Sync<T1, T2> sync_;
  ros::NodeHandle nh_;
  std::vector<std::shared_ptr<Topic<T1, Sync<T1, T2>>>> topics1_;
  std::vector<std::shared_ptr<Topic<T2, Sync<T1, T2>>>> topics2_;
};

// ------------ subscriber sync for three different type of topics -----

template <class T1, class T2, class T3>
class SubscribingSync<T1, T2, T3>
{
public:
  using string = std::string;
  using Time = rclcpp::Time;
  typedef Sync<T1, T2, T3> Sync3;
  typedef typename Sync<T1, T2, T3>::Callback Callback;

  SubscribingSync(
    const ros::NodeHandle & nh, const std::vector<std::vector<string>> & topics,
    const Callback & callback, unsigned int maxQueueSize = 5)
  : sync_(
      std::vector<std::vector<string>>(3, std::vector<string>()), callback,
      maxQueueSize),
    nh_(nh)
  {
    if (topics.size() > 0) {
      for (const auto & topic : topics[0]) {
        addTopic1(topic);
      }
    }
    if (topics.size() > 1) {
      for (const auto & topic : topics[1]) {
        addTopic2(topic);
      }
    }
    if (topics.size() > 2) {
      for (const auto & topic : topics[2]) {
        addTopic3(topic);
      }
    }
  }
  void addTopic1(const std::string & t)
  {
    topics1_.emplace_back(new Topic<T1, Sync3>(t, nh_, sync_.qs(), &sync_));
    sync_.addTopic1(t);
  }
  void addTopic2(const std::string & t)
  {
    topics2_.emplace_back(new Topic<T2, Sync3>(t, nh_, sync_.qs(), &sync_));
    sync_.addTopic2(t);
  }
  void addTopic3(const std::string & t)
  {
    topics3_.emplace_back(new Topic<T3, Sync3>(t, nh_, sync_.qs(), &sync_));
    sync_.addTopic3(t);
  }

private:
  Sync<T1, T2, T3> sync_;
  ros::NodeHandle nh_;
  std::vector<std::shared_ptr<Topic<T1, Sync<T1, T2, T3>>>> topics1_;
  std::vector<std::shared_ptr<Topic<T2, Sync<T1, T2, T3>>>> topics2_;
  std::vector<std::shared_ptr<Topic<T3, Sync<T1, T2, T3>>>> topics3_;
};
}  // namespace flex_sync
