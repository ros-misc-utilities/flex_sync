// -*-c++-*--------------------------------------------------------------------
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

#include <gtest/gtest.h>

#include <flex_sync/approximate_sync.hpp>
#include <flex_sync/exact_sync.hpp>
#include <std_msgs/msg/header.hpp>

using std_msgs::msg::Header;

class CallbackTest
{
public:
  void cb3(
    const std::vector<Header::ConstSharedPtr> & msgvec1,
    const std::vector<Header::ConstSharedPtr> & msgvec2,
    const std::vector<Header::ConstSharedPtr> & msgvec3)
  {
    (void)msgvec1;
    (void)msgvec2;
    (void)msgvec3;
  }
};

TEST(flex_sync, exact_sync_compiles)
{
  CallbackTest cbt;

  std::function<void(
    const std::vector<Header::ConstSharedPtr> &,
    const std::vector<Header::ConstSharedPtr> &,
    const std::vector<Header::ConstSharedPtr> &)>
    cb = std::bind(
      &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3);

  const std::vector<std::vector<std::string>> topics = {
    {"a_topic_1", "a_topic_2"}, {"b_topic_1"}, {"c_topic_1", "c_topic_2"}};
  flex_sync::ExactSync<Header, Header, Header> sync(topics, cb, 10);
}

TEST(flex_sync, approximate_sync_compiles)
{
  CallbackTest cbt;

  std::function<void(
    const std::vector<Header::ConstSharedPtr> &,
    const std::vector<Header::ConstSharedPtr> &,
    const std::vector<Header::ConstSharedPtr> &)>
    cb = std::bind(
      &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3);

  const std::vector<std::vector<std::string>> topics = {
    {"a_topic_1", "a_topic_2"}, {"b_topic_1"}, {"c_topic_1", "c_topic_2"}};
  flex_sync::ApproximateSync<Header, Header, Header> sync(topics, cb, 10);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
