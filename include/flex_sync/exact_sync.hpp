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
#ifndef FLEX_SYNC__EXACT_SYNC_HPP_
#define FLEX_SYNC__EXACT_SYNC_HPP_

#include <deque>
#include <flex_sync/msg_pack.hpp>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

/*
 * Class for exact synchronizing across variable number of messages
 */

namespace flex_sync
{
template <typename... MsgTypes>
class ExactSync
{
private:
  /**
  * \cond HIDDEN_SYMBOLS
  */
  // CallbackArg has arguments of the callback as a tuple
  typedef std::tuple<std::vector<std::shared_ptr<const MsgTypes>>...>
    CallbackArg;
  // A time slot aggregates all messages for that time
  struct TimeSlot
  {
    int num_valid_messages{0};
    CallbackArg candidate;
  };
  // TypeInfo holds all data for a particular message type
  template <typename MsgType>
  struct TypeInfo
  {
    std::map<std::string, int> topic_to_index;
  };
  typedef std::map<rclcpp::Time, std::shared_ptr<TimeSlot>> TimeToSlot;
  typedef std::tuple<TypeInfo<const MsgTypes>...> TupleOfTypeInfo;
  /**
  * \endcond
  */

public:
  /** The type of callback to expect from this sync.
   * A typical callback function signature would be
   * ```
   *     void foo(const std::vector<std::shared_ptr<const MsgType>> &vec1,
   *              const std::vector<std::shared_ptr<const MsgType>> &vec2)
   * ```
   */
  using Callback = std::function<void(
    const std::vector<std::shared_ptr<const MsgTypes>> &...)>;

  /**
  * Creates an exact sync like the one in ROS1, but with
  *  flexible number of topics per type.
  * \param topics vector of vector of topics, corresponding to message type.
  *               The first vector has all topics for type 1, second for type 2 etc.
  * \param cb callback to invoke when sync is obtained.
  *           The callback signature looks different. For example
  *           for two message types, MsgType1 and MsgType2
  * ```
  *           void callback(
  *                  const std::vector<MsgType1::ConstPtr> &m1,
  *                  const std::vector<MsgType2::ConstPtr> &m2);
  * ```
  * \param queue_size depth of sync queue. A ``queue_size`` of 0 means unlimited queue.
  */
  ExactSync(
    const std::vector<std::vector<std::string>> & topics, Callback cb,
    size_t queue_size)
  : topics_(topics), cb_(cb), queue_size_(queue_size)
  {
    const size_t ntypes = sizeof...(MsgTypes);
    if (ntypes != topics.size()) {
      RCLCPP_ERROR_STREAM(
        rclcpp::get_logger("exact_sync"),
        "exact sync: number of topic vectors: "
          << topics.size()
          << " does not match number of message types: " << ntypes);
      throw(std::runtime_error("num topic vectors != num msg types"));
    }
    TopicIndexInitializer tii;
    (void)for_each(type_infos_, &tii);
  }

  /**
   * Query the number of messages dropped because they were not synced.
   * \return total number of dropped messages since last
   *         call to \ref clearNumberDropped()
  */
  size_t getNumberDropped() const { return (num_dropped_); }

  /**
  * Clears number of dropped messages to zero. The number
  * of dropped messages can be queried with \ref getNumberDropped().
  */
  void clearNumberDropped() { num_dropped_ = 0; }

  /**
   * Get topics with which this sync was created.
   * \return topics with which this sync was created.
  */
  const std::vector<std::vector<std::string>> & getTopics() const
  {
    return (topics_);
  }

  /**
   * Query the sync queue size.
   * \return size of sync queue
  */
  size_t getQueueSize() const { return (queue_size_); }

  /**
   * Call this method to feed data into the sync. Once a sync
   * is obtained (all topics have messages for a given time),
   * the sync will invoke the callback function.
   * 
   * \param topic topic of the message to be processed. Must
   *              be one of the topics provided when the sync
   *              was created.
   * \param msg shared pointer to message to be processed. Message
   *            type must match positionally to topic used when
   *            the sync was created.
   */
  template <typename MsgPtrT>
  void process(const std::string & topic, const MsgPtrT msg)
  {
    using TypeInfoT = TypeInfo<typename MsgPtrT::element_type const>;
    using VecT =
      std::vector<std::shared_ptr<typename MsgPtrT::element_type const>>;
    const rclcpp::Time & t = msg->header.stamp;
    auto it = time_to_slot_.find(t);
    if (it == time_to_slot_.end()) {
      std::shared_ptr<TimeSlot> slot = makeTimeSlot();
      it = time_to_slot_.insert({t, slot}).first;
    }
    // find correct topic info via lookup by type
    TypeInfoT & ti = std::get<TypeInfoT>(type_infos_);
    auto topic_it = ti.topic_to_index.find(topic);
    if (topic_it == ti.topic_to_index.end()) {
      RCLCPP_ERROR_STREAM(
        rclcpp::get_logger("exact_sync"),
        "flex_sync: invalid topic " << topic << " for message type");
      throw std::runtime_error("invalid topic: " + topic);
    }
    // from looked-up tuple, grab the right type
    TimeSlot & slot = *(it->second);
    VecT & msg_vec = std::get<VecT>(slot.candidate);
    if (!msg_vec[topic_it->second]) {
      msg_vec[topic_it->second] = msg;  // save message
      slot.num_valid_messages++;
    }
    if (slot.num_valid_messages == tot_num_topics_) {
      // deliver callback
      std::apply([this](auto &&... args) { cb_(args...); }, slot.candidate);
      // clear this and all old tuples
      auto itpp = it;
      itpp++;
      while (time_to_slot_.begin() != itpp) {
        if (time_to_slot_.begin() != it) {
          num_dropped_ += (time_to_slot_.begin()->second)->num_valid_messages;
        }
        time_to_slot_.erase(time_to_slot_.begin());
      }
    }
    if (queue_size_ > 0) {
      while (time_to_slot_.size() > queue_size_) {
        num_dropped_ += (time_to_slot_.begin()->second)->num_valid_messages;
        time_to_slot_.erase(time_to_slot_.begin());
      }
    }
  }
  /**
   * Exposes the message types.
  */
  using message_types = MsgPack<MsgTypes...>;

private:
  /**
  * \cond HIDDEN_SYMBOLS
  */

  struct TopicIndexInitializer
  {
    template <std::size_t I>
    int operate(ExactSync<MsgTypes...> * sync) const
    {
      const size_t num_topics = sync->topics_[I].size();
      auto & type_info = std::get<I>(sync->type_infos_);
      sync->tot_num_topics_ += num_topics;
      // make map between topic string and index for
      // lookup when data arrives
      for (int t_idx = 0; t_idx < static_cast<int>(sync->topics_[I].size());
           t_idx++) {
        type_info.topic_to_index[sync->topics_[I][t_idx]] = t_idx;
      }
      return (num_topics);
    }
  };

  class TimeSlotMaker
  {
  public:
    TimeSlotMaker() { timeSlot_.reset(new TimeSlot()); }
    template <std::size_t I>
    int operate(ExactSync<MsgTypes...> * sync) const
    {
      auto & type_info = std::get<I>(sync->type_infos_);
      auto & cand = std::get<I>(timeSlot_->candidate);
      cand.resize(type_info.topic_to_index.size());
      return (0);
    }
    std::shared_ptr<TimeSlot> getTimeSlot() const { return (timeSlot_); }

  private:
    std::shared_ptr<TimeSlot> timeSlot_;
  };

  // TODO(Bernd): this may be a slow operation, maybe we create it once
  // on startup and make copy afterwards
  std::shared_ptr<TimeSlot> makeTimeSlot()
  {
    TimeSlotMaker tm;
    (void)for_each(type_infos_, &tm);
    return (tm.getTimeSlot());
  }

  // some neat template tricks picked up here:
  // https://stackoverflow.com/questions/18063451/get-index-of-a-tuple-elements -type
  // This template terminates the recursion
  template <std::size_t I = 0, typename FuncT, typename... Tp>
  inline typename std::enable_if<I == sizeof...(Tp), int>::type for_each(
    std::tuple<Tp...> &, FuncT *)  // Unused arg needs no name
  {
    return 0;
  }  // do nothing

  // This template recursively calls itself, thereby iterating
  template <std::size_t I = 0, typename FuncT, typename... Tp>
    inline typename std::enable_if <
    I<sizeof...(Tp), int>::type for_each(std::tuple<Tp...> & t, FuncT * f)
  {
    const int rv = (*f).template operate<I>(this);
    return (rv + for_each<I + 1, FuncT, Tp...>(t, f));
  }
  /**
  * \endcond
  */

  // ----------- variables -----------------------
  std::vector<std::vector<std::string>> topics_;  // topics to be synced
  Callback cb_;                                   // pointer to the callee
  size_t queue_size_{0};        // keep at most this number of time stamps
  TimeToSlot time_to_slot_;     // maps header time to slot
  TupleOfTypeInfo type_infos_;  // tuple with per-msg-type topic maps
  int tot_num_topics_{0};       // for deciding when time slot is complete
  size_t num_dropped_{0};       // total number of dropped messages
};
}  // namespace flex_sync

#endif  // FLEX_SYNC__EXACT_SYNC_HPP_
