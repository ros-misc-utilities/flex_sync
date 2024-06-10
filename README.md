# flex_sync

This ROS2 package implements a message synchronization filter. It is similar to the
well-known [message_filters](https://github.com/ros2/message_filters) package, but is
more flexible in that the number of topics to be synchronized does not have to be known
at compile time, just their message types. This can be useful when you don't know
beforehand how many sensors of a given type will be on the robot.

Just like [message_filters](https://github.com/ros2/message_filters), ``flex_sync`` offers
exact and approximate synchronization policies.

## How to use
Here is code snippet that shows how to perform an exact synchronization between two
Image messages and one Imu message:

```c++
    using sensor_msg::msg::Image;
    using sensor_msg::msg::Imu;
    using CallbackType = std::function<void(const std::vector<Image::ConstSharedPtr> &,
            const std::vector<Imu::ConstSharedPtr> &)>;

    class MyTest { // define class to handle callbacks
        public:
        void callback(const std::vector<Image::ConstSharedPtr> & msgvec1,
                      const std::vector<Imu::ConstSharedPtr> & msgvec2) {
            // should print out "got msgs: 2 + 1"
            std::cout << "got msgs: " << msgvec1.size() << " + " << msgvec2.size() << std::endl;
        }
    };
    MyTest my_test; // instantiate object to handle synchronized callbacks
    CallbackType cb = std::bind(  // bind to class member function
        &MyTest::callback, &my_test, std::placeholders::_1, std::placeholders::_2);

    // synchronize two Image topics and one Imu topic
    const std::vector<std::vector<std::string>> topics =
       {{"image_topic_1", "image_topic_2"}, {"imu_topic_1"}};
    const size_t q_size = 10; // depth of sync queue
    flex_sync::ExactSync<Image, Imu> sync(topics, cb, q_size);
    // now feed images and IMU messages into the sync. If
    // the sync is successful there will be callbacks to MyTest::callback()
    sync->process("image_topic_1", std::make_shared<Image>()); // replace with valid message
    sync->process("image_topic_2", std::make_shared<Image>());
    sync->process("imu_topic_1", std::make_shared<Imu>());
```
Note that the number of topics does *not* have to be known at compile time, but it cannot
change during the life time of the sync object.

## Special note from the author

The code in this repo is absolutely hideous. The template syntax is cryptic to begin with, and I don't use
templates often enough to become good at them. Don't ask me any questions on how this code works, I no longer
understand it myself. It certainly could use some cleaning up by somebody who knows templates better.
Having that said, I have not discovered any obvious bugs so far and I use it in two projects.

If you want to understand better how the sync code works, look at the
[ROS1 documentation](https://wiki.ros.org/message_filters) and in particular the adaptive algorithm for
the [approximate time sync](https://wiki.ros.org/message_filters/ApproximateTime).


## License

This software and any future contributions to it are licensed under
the [Apache License 2.0](LICENSE).
