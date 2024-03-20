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
#include <vector>

using std_msgs::msg::Header;
using Topics = std::vector<std::vector<std::string>>;

enum MsgType { DUMMY_1, DUMMY_2, DUMMY_3 };

struct Dummy
{
  explicit Dummy(MsgType typ) : type(typ) {}
  MsgType type;
  Header header;
};

struct Dummy1 : public Dummy
{
  using ConstSharedPtr = std::shared_ptr<const Dummy1>;
  Dummy1() : Dummy(DUMMY_1) {}
};

struct Dummy2 : public Dummy
{
  using ConstSharedPtr = std::shared_ptr<const Dummy2>;
  Dummy2() : Dummy(DUMMY_2) {}
};

struct Dummy3 : public Dummy
{
  using ConstSharedPtr = std::shared_ptr<const Dummy3>;
  Dummy3() : Dummy(DUMMY_3) {}
};

using CallbackType = std::function<void(
  const std::vector<Dummy1::ConstSharedPtr> &,
  const std::vector<Dummy2::ConstSharedPtr> &,
  const std::vector<Dummy3::ConstSharedPtr> &)>;

struct MsgInfo
{
  explicit MsgInfo(MsgType typ, uint64_t t, const std::string & tp) : topic(tp)
  {
    switch (typ) {
      case DUMMY_1:
        message.reset(new Dummy1());
        break;
      case DUMMY_2:
        message.reset(new Dummy2());
        break;
      case DUMMY_3:
        message.reset(new Dummy3());
        break;
    }
    message->header.stamp =
      rclcpp::Time(static_cast<int64_t>(t), RCL_SYSTEM_TIME);
  }

  std::string topic;
  std::shared_ptr<Dummy> message;
};

static std::vector<MsgInfo> make_messages(
  uint64_t start_time, uint64_t end_time, uint64_t dt, const Topics & topics)
{
  std::vector<MsgInfo> msgs;
  for (uint64_t t = start_time; t < end_time; t += dt) {
    for (size_t i = 0; i < topics.size(); i++) {
      const auto & mtype = topics[i];
      for (const auto & topic : mtype) {
        msgs.push_back(MsgInfo(static_cast<MsgType>(i), t, topic));
      }
    }
  }
  return (msgs);
}

class CallbackTest
{
public:
  void cb3(
    const std::vector<Dummy1::ConstSharedPtr> & msgvec1,
    const std::vector<Dummy2::ConstSharedPtr> & msgvec2,
    const std::vector<Dummy3::ConstSharedPtr> & msgvec3)
  {
#ifdef PRINT_DEBUG
    std::cout << "------------- callback: --------- " << std::endl;
    std::cout << "mv1:";
    for (const auto & m : msgvec1) {
      std::cout << " " << rclcpp::Time(m->header.stamp).nanoseconds();
    }
    std::cout << std::endl;
    std::cout << "mv2:";
    for (const auto & m : msgvec2) {
      std::cout << " " << rclcpp::Time(m->header.stamp).nanoseconds();
    }
    std::cout << std::endl;
    std::cout << "mv3:";
    for (const auto & m : msgvec3) {
      std::cout << " " << rclcpp::Time(m->header.stamp).nanoseconds();
    }
    std::cout << std::endl;
#endif
    num_callbacks_++;
    num_msgs_ += msgvec1.size() + msgvec2.size() + msgvec3.size();
  }
  size_t getNumberOfCallbacks() const { return (num_callbacks_); }
  size_t getNumberOfMessages() const { return (num_msgs_); }

private:
  size_t num_callbacks_{0};
  size_t num_msgs_{0};
};

static const Topics topics_3 = {
  {"a_topic_1", "a_topic_2"}, {"b_topic_1"}, {"c_topic_1", "c_topic_2"}};

TEST(flex_sync, exact_sync_compiles)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);

  flex_sync::ExactSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
}

TEST(flex_sync, approximate_sync_compiles)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);
  flex_sync::ApproximateSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
}

template <class T>
static void playMessages(T * sync, const std::vector<MsgInfo> & messages)
{
  for (const auto & msg : messages) {
    switch (msg.message->type) {
      case DUMMY_1:
        sync->process(
          msg.topic, std::reinterpret_pointer_cast<Dummy1>(msg.message));
        break;
      case DUMMY_2:
        sync->process(
          msg.topic, std::reinterpret_pointer_cast<Dummy2>(msg.message));
        break;
      case DUMMY_3:
        sync->process(
          msg.topic, std::reinterpret_pointer_cast<Dummy3>(msg.message));
        break;
    }
  }
}

TEST(flex_sync, exact_sync_few_messages)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);

  flex_sync::ExactSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
  auto messages = make_messages(0ULL, 10ULL, 1ULL, topics_3);
  playMessages(&sync, messages);
  EXPECT_EQ(cbt.getNumberOfCallbacks(), static_cast<size_t>(10));
  EXPECT_EQ(cbt.getNumberOfMessages(), static_cast<size_t>(50));
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
