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
#include <flex_sync/live_sync.hpp>
#include <geometry_msgs/msg/accel_stamped.hpp>
#include <geometry_msgs/msg/vector3_stamped.hpp>
#include <std_msgs/msg/header.hpp>
#include <vector>

// #define PRINT_DEBUG

using std_msgs::msg::Header;
using V3 = geometry_msgs::msg::Vector3Stamped;
using AS = geometry_msgs::msg::AccelStamped;
using Topics = std::vector<std::vector<std::string>>;
using DeltaTimes = std::vector<std::vector<uint64_t>>;

enum MsgType { DUMMY_1, DUMMY_2, DUMMY_3 };

static unsigned int seed(0);  // random seed

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

using CallbackType1 =
  std::function<void(const std::vector<V3::ConstSharedPtr>)>;

using CallbackType2 = std::function<void(
  const std::vector<V3::ConstSharedPtr> &,
  const std::vector<AS::ConstSharedPtr> &)>;

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

static std::vector<MsgInfo> make_simple_messages(
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

static std::vector<MsgInfo> make_random_messages(
  uint64_t start_time, uint64_t end_time,
  const std::vector<std::vector<uint64_t>> & dt, uint64_t random_delay,
  const Topics & topics)
{
  std::vector<MsgInfo> msgs;
  srand(1);

  // initialize curr_times for all queues
  std::vector<std::vector<uint64_t>> curr_times;
  for (size_t type_idx = 0; type_idx < topics.size(); type_idx++) {
    curr_times.push_back(
      std::vector<uint64_t>(topics[type_idx].size(), start_time));
  }

  for (uint64_t t = start_time; t < end_time; t++) {
    for (size_t type_idx = 0; type_idx < topics.size(); type_idx++) {
      const auto & mtype = topics[type_idx];
      for (size_t topic_idx = 0; topic_idx < mtype.size(); topic_idx++) {
        const auto & topic = mtype[topic_idx];
        // advance this queue beyond t
        auto & curr_time = curr_times[type_idx][topic_idx];
        if (curr_time <= t) {
          const int rd = random_delay * (rand_r(&seed) % 2);
          msgs.push_back(
            MsgInfo(static_cast<MsgType>(type_idx), curr_time + rd, topic));
          curr_time += dt[type_idx][topic_idx];
        }
      }
    }
  }

  return (msgs);
}

class CallbackTest
{
public:
  void cb1(std::vector<V3::ConstSharedPtr> msgvec1)
  {
    num_callbacks_++;
    num_msgs_ += msgvec1.size();
  }
  void cb2(
    const std::vector<V3::ConstSharedPtr> & msgvec1,
    const std::vector<AS::ConstSharedPtr> & msgvec2)
  {
    num_callbacks_++;
    num_msgs_ += msgvec1.size() + msgvec2.size();
  }

  void cb3(
    const std::vector<Dummy1::ConstSharedPtr> & msgvec1,
    const std::vector<Dummy2::ConstSharedPtr> & msgvec2,
    const std::vector<Dummy3::ConstSharedPtr> & msgvec3)
  {
#ifdef PRINT_DEBUG
    std::cout << "----------------- callback: " << num_callbacks_ << std::endl;
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

#ifdef PRINT_DEBUG
static void printMessages(const std::vector<MsgInfo> & messages)
{
  for (const auto & m : messages) {
    std::cout << rclcpp::Time(m.message->header.stamp).nanoseconds() << " "
              << m.topic << std::endl;
  }
}
#endif

static const Topics topics_1 = {{"a_topic_1", "a_topic_2"}};

static const Topics topics_2 = {{"a_topic_1", "a_topic_2"}, {"b_topic_1"}};

static const Topics topics_3 = {
  {"a_topic_1", "a_topic_2"}, {"b_topic_1"}, {"c_topic_1", "c_topic_2"}};

static const DeltaTimes dt_3 = {{2, 2}, {3}, {2, 4}};
static const DeltaTimes dt_3_10 = {{20, 20}, {30}, {20, 40}};

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

TEST(flex_sync, exact_sync_simple_messages)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);

  flex_sync::ExactSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
  auto messages = make_simple_messages(0ULL, 10ULL, 1ULL, topics_3);
  playMessages(&sync, messages);
  EXPECT_EQ(cbt.getNumberOfCallbacks(), static_cast<size_t>(10));
  EXPECT_EQ(cbt.getNumberOfMessages(), static_cast<size_t>(50));
}

TEST(flex_sync, approx_sync_simple_messages)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);

  flex_sync::ApproximateSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
  auto messages = make_simple_messages(0ULL, 10ULL, 1ULL, topics_3);
  playMessages(&sync, messages);
  EXPECT_EQ(cbt.getNumberOfCallbacks(), static_cast<size_t>(10));
  EXPECT_EQ(cbt.getNumberOfMessages(), static_cast<size_t>(50));
}

TEST(flex_sync, approx_sync_different_freq)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);

  flex_sync::ApproximateSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
  // longest period dt = 4 ->  time stamps 0, 4, 8, 12, 16
  auto messages = make_random_messages(0ULL, 20ULL, dt_3, 0ULL, topics_3);
#ifdef PRINT_DEBUG
  printMessages(messages);
#endif
  playMessages(&sync, messages);
  EXPECT_EQ(cbt.getNumberOfCallbacks(), static_cast<size_t>(5));
  EXPECT_EQ(cbt.getNumberOfMessages(), static_cast<size_t>(25));
}

TEST(flex_sync, approx_sync_random_delay)
{
  CallbackTest cbt;
  CallbackType cb = std::bind(
    &CallbackTest::cb3, &cbt, std::placeholders::_1, std::placeholders::_2,
    std::placeholders::_3);

  flex_sync::ApproximateSync<Dummy1, Dummy2, Dummy3> sync(topics_3, cb, 10);
  auto messages = make_random_messages(0ULL, 200ULL, dt_3_10, 3ULL, topics_3);
#ifdef PRINT_DEBUG
  printMessages(messages);
#endif
  playMessages(&sync, messages);
  EXPECT_EQ(cbt.getNumberOfCallbacks(), static_cast<size_t>(5));
  EXPECT_EQ(cbt.getNumberOfMessages(), static_cast<size_t>(25));
}

TEST(flex_sync, exact_live_sync)
{
  CallbackTest cbt;
  CallbackType2 cb2 = std::bind(
    &CallbackTest::cb2, &cbt, std::placeholders::_1, std::placeholders::_2);
  using MySync = flex_sync::ExactSync<V3, AS>;
  rclcpp::Node node("test", rclcpp::NodeOptions());
  flex_sync::LiveSync<MySync> sync(&node, topics_2, cb2, 10);
}

static struct InitRos
{
  InitRos() { rclcpp::init(0, 0); }
} ros_init;

TEST(flex_sync, approx_live_sync)
{
  CallbackTest cbt;
  CallbackType2 cb2 = std::bind(
    &CallbackTest::cb2, &cbt, std::placeholders::_1, std::placeholders::_2);
  using MySync = flex_sync::ApproximateSync<V3, AS>;
  rclcpp::Node node("test", rclcpp::NodeOptions());
  flex_sync::LiveSync<MySync> sync(&node, topics_2, cb2, 10);
}

// test template specialization for subscriber

namespace flex_sync
{
template <typename SyncT>
class Subscriber<SyncT, V3>
{
public:
  Subscriber(
    const std::string &, rclcpp::Node *, const rclcpp::QoS &,
    const std::shared_ptr<SyncT> &)
  {
  }
};
}  // namespace flex_sync

TEST(flex_sync, approx_live_sync_templated)
{
  CallbackTest cbt;
  CallbackType2 cb2 = std::bind(
    &CallbackTest::cb2, &cbt, std::placeholders::_1, std::placeholders::_2);
  using MySync = flex_sync::ApproximateSync<V3, AS>;
  rclcpp::Node node("test", rclcpp::NodeOptions());
  flex_sync::LiveSync<MySync> sync(&node, topics_2, cb2, rclcpp::QoS(10));
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
