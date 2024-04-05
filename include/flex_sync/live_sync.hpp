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

#ifndef FLEX_SYNC__LIVE_SYNC_HPP_
#define FLEX_SYNC__LIVE_SYNC_HPP_

#include <flex_sync/msg_pack.hpp>
#include <rclcpp/rclcpp.hpp>

/* ------------------------------------------------
* Replace or specialize this default template to
* handle subscription to your liking
*/

namespace flex_sync
{
template <typename SyncT, typename T>
class Subscriber
{
public:
  using TConstSharedPtr = typename T::ConstSharedPtr;

  Subscriber(
    const std::string & topic, rclcpp::Node * node, const rclcpp::QoS & qos,
    const std::shared_ptr<SyncT> & sync)
  : topic_(topic), sync_(sync)
  {
    sub_ = node->create_subscription<T>(
      topic, qos,
      std::bind(&Subscriber::callback, this, std::placeholders::_1));
  }
  void callback(TConstSharedPtr msg) { sync_->process(topic_, msg); }

private:
  std::string topic_;
  std::shared_ptr<SyncT> sync_;
  typename rclcpp::Subscription<T>::SharedPtr sub_;
};

/* ------------------- HERE STARTS THE MAIN CLASS ------- */

// use this trick to get to the parameter pack
// https://stackoverflow.com/questions/22968182/
// is-it-possible-to-typedef-a-parameter-pack

template <
  typename SyncT,
  template <typename, typename> typename SubscriberT = Subscriber,
  typename = typename SyncT::message_types>
class LiveSync;

// now partial specialization
template <
  typename SyncT, template <typename, typename> typename SubscriberT,
  typename... MsgTypes>
class LiveSync<SyncT, SubscriberT, MsgPack<MsgTypes...>>
{
public:
  using string = std::string;
  using Time = rclcpp::Time;
  typedef std::tuple<
    std::vector<std::shared_ptr<SubscriberT<SyncT, MsgTypes>>>...>
    TupleOfTopicVec;
  typedef typename SyncT::Callback Callback;

  LiveSync(
    rclcpp::Node * node, const std::vector<std::vector<string>> & topics,
    const Callback & callback, const rclcpp::QoS & qos)
  : node_(node)
  {
    sync_.reset(new SyncT(topics, callback, std::max(size_t(5), qos.depth())));
    // initialize topics
    TopicInitializer ti;
    (void)for_each(topics_, &ti);
  }

  std::shared_ptr<SyncT> getSync() { return (sync_); }

  rclcpp::Node * getNode() { return (node_); }

private:
  struct TopicInitializer
  {
    template <std::size_t I>
    int operate(LiveSync<SyncT, SubscriberT> * liveSync) const
    {
      std::shared_ptr<SyncT> sync = liveSync->getSync();
      auto & topics = sync->getTopics();
      auto & topic_vec = std::get<I>(liveSync->topics_);
      // std::cout << "creating topic for " << I
      //<< " num: " << topics[I].size() << std::endl;
      auto node = liveSync->getNode();
      const unsigned int qs = sync->getQueueSize();
      for (const std::string & topic : topics[I]) {
        // get vector type-> pointer type -> pointer element type
        typedef
          typename get_type<I, TupleOfTopicVec>::type::value_type::element_type
            SubscriberET;
        std::shared_ptr<SubscriberET> lt(
          new SubscriberET(topic, node, qs, sync));
        topic_vec.push_back(lt);
      }
      return (topic_vec.size());
    }
  };

  // some template tricks picked up here:
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

  // The following templates return the N'th type of a tuple.
  // source:
  // https://stackoverflow.com/questions/16928669/how-to-get-n-th-type-from-a-tuple
  template <int N, typename... Ts>
  struct get_type;
  template <int N, typename T, typename... Ts>
  struct get_type<N, std::tuple<T, Ts...>>
  {
    using type = typename get_type<N - 1, std::tuple<Ts...>>::type;
  };
  template <typename T, typename... Ts>
  struct get_type<0, std::tuple<T, Ts...>>
  {
    using type = T;
  };

  // -------------- variables -------------
  std::shared_ptr<SyncT> sync_;
  TupleOfTopicVec topics_;
  rclcpp::Node * node_{nullptr};
};
}  // namespace flex_sync

#endif  // FLEX_SYNC__LIVE_SYNC_HPP_
