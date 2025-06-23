// Copyright (c) 2022, Stogl Robotics Consulting UG (haftungsbeschränkt)
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

#ifndef CANOPEN_ROS2_CONTROLLERS__CONTROLLER__TEST_CANOPEN_PROXY_CONTROLLER_HPP_
#define CANOPEN_ROS2_CONTROLLERS__CONTROLLER__TEST_CANOPEN_PROXY_CONTROLLER_HPP_

#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "canopen_ros2_controllers/canopen_proxy_controller.hpp"
#include "gmock/gmock.h"
#include "hardware_interface/loaned_command_interface.hpp"
#include "hardware_interface/loaned_state_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/parameter_value.hpp"
#include "rclcpp/time.hpp"
#include "rclcpp/utilities.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"

// TODO(anyone): replace the state and command message types
using ControllerStateMsg = canopen_ros2_controllers::CanopenProxyController::ControllerNMTStateMsg;
using ControllerCommandMsg = canopen_ros2_controllers::CanopenProxyController::ControllerCommandMsg;
using ControllerModeSrvType =
  canopen_ros2_controllers::CanopenProxyController::ControllerStartResetSrvType;

namespace
{
constexpr auto NODE_SUCCESS = controller_interface::CallbackReturn::SUCCESS;
constexpr auto NODE_ERROR = controller_interface::CallbackReturn::ERROR;
}  // namespace

// subclassing and friending so we can access member variables
class TestableCanopenProxyController : public canopen_ros2_controllers::CanopenProxyController
{
  FRIEND_TEST(CanopenProxyControllerTest, joint_names_parameter_not_set);
  FRIEND_TEST(CanopenProxyControllerTest, all_parameters_set_configure_success);
  FRIEND_TEST(CanopenProxyControllerTest, activate_success);
  FRIEND_TEST(CanopenProxyControllerTest, reactivate_success);
  FRIEND_TEST(CanopenProxyControllerTest, test_setting_slow_mode_service);
  FRIEND_TEST(CanopenProxyControllerTest, test_update_logic_fast);
  FRIEND_TEST(CanopenProxyControllerTest, test_update_logic_slow);

public:
  controller_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override
  {
    auto ret = canopen_ros2_controllers::CanopenProxyController::on_configure(previous_state);
    // Only if on_configure is successful create subscription
    if (ret == CallbackReturn::SUCCESS)
    {
      cmd_subscriber_wait_set_.add_subscription(tpdo_subscriber_);
    }
    return ret;
  }

  /**
   * @brief wait_for_command blocks until a new ControllerCommandMsg is received.
   * Requires that the executor is not spinned elsewhere between the
   *  message publication and the call to this function.
   *
   * @return true if new ControllerCommandMsg msg was received, false if timeout.
   */
  bool wait_for_command(
    rclcpp::Executor & executor, rclcpp::WaitSet & subscriber_wait_set,
    const std::chrono::milliseconds & timeout = std::chrono::milliseconds{500})
  {
    bool success = subscriber_wait_set.wait(timeout).kind() == rclcpp::WaitResultKind::Ready;
    if (success)
    {
      executor.spin_some();
    }
    return success;
  }

  bool wait_for_commands(
    rclcpp::Executor & executor,
    const std::chrono::milliseconds & timeout = std::chrono::milliseconds{500})
  {
    return wait_for_command(executor, cmd_subscriber_wait_set_, timeout);
  }

  // TODO(anyone): add implementation of any methods of your controller is needed

private:
  rclcpp::WaitSet cmd_subscriber_wait_set_;
};

// We are using template class here for easier reuse of Fixture in specializations of controllers
template <typename CtrlType>
class CanopenProxyControllerFixture : public ::testing::Test
{
public:
  static void SetUpTestCase() { rclcpp::init(0, nullptr); }

  void SetUp()
  {
    // initialize controller
    controller_ = std::make_unique<CtrlType>();

    command_publisher_node_ = std::make_shared<rclcpp::Node>("command_publisher");
    command_publisher_ = command_publisher_node_->create_publisher<ControllerCommandMsg>(
      "/test_canopen_ros2_controllers/commands", rclcpp::SystemDefaultsQoS());

    service_caller_node_ = std::make_shared<rclcpp::Node>("service_caller");
    slow_control_service_client_ = service_caller_node_->create_client<ControllerModeSrvType>(
      "/test_canopen_ros2_controllers/set_slow_control_mode");
  }

  static void TearDownTestCase() { rclcpp::shutdown(); }

  void TearDown() { controller_.reset(nullptr); }

protected:
  void SetUpController(
    bool set_parameters = true, std::string controller_name = "test_canopen_ros2_controllers")
  {
    ASSERT_EQ(controller_->init(controller_name), controller_interface::return_type::OK);

    std::vector<hardware_interface::LoanedCommandInterface> command_ifs;
    command_itfs_.reserve(joint_command_values_.size());
    command_ifs.reserve(joint_command_values_.size());

    for (size_t i = 0; i < joint_command_values_.size(); ++i)
    {
      command_itfs_.emplace_back(hardware_interface::CommandInterface(
        joint_name_, command_interface_names_[i], &joint_command_values_[i]));
      command_ifs.emplace_back(command_itfs_.back());
    }

    std::vector<hardware_interface::LoanedStateInterface> state_ifs;
    state_itfs_.reserve(joint_state_values_.size());
    state_ifs.reserve(joint_state_values_.size());

    for (size_t i = 0; i < joint_state_values_.size(); ++i)
    {
      state_itfs_.emplace_back(hardware_interface::StateInterface(
        joint_name_, state_interface_names_[i], &joint_state_values_[i]));
      state_ifs.emplace_back(state_itfs_.back());
    }

    controller_->assign_interfaces(std::move(command_ifs), std::move(state_ifs));

    if (set_parameters)
    {
      controller_->get_node()->set_parameter({"joint", joint_name_});
    }
  }

  void subscribe_and_get_messages(ControllerStateMsg & msg)
  {
    // create a new subscriber
    rclcpp::Node test_subscription_node("test_subscription_node");
    auto subs_callback = [&](const ControllerStateMsg::SharedPtr) {};
    auto subscription = test_subscription_node.create_subscription<ControllerStateMsg>(
      "/test_canopen_ros2_controllers/state", 10, subs_callback);

    // call update to publish the test value
    ASSERT_EQ(
      controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
      controller_interface::return_type::OK);

    // call update to publish the test value
    // since update doesn't guarantee a published message, republish until received
    int max_sub_check_loop_count = 5;  // max number of tries for pub/sub loop
    rclcpp::WaitSet wait_set;          // block used to wait on message
    wait_set.add_subscription(subscription);
    while (max_sub_check_loop_count--)
    {
      controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01));
      // check if message has been received
      if (wait_set.wait(std::chrono::milliseconds(2)).kind() == rclcpp::WaitResultKind::Ready)
      {
        break;
      }
    }
    ASSERT_GE(max_sub_check_loop_count, 0) << "Test was unable to publish a message through "
                                              "controller/broadcaster update loop";

    // take message from subscription
    rclcpp::MessageInfo msg_info;
    ASSERT_TRUE(subscription->take(msg, msg_info));
  }

  // TODO(anyone): add/remove arguments as it suites your command message type
  void publish_commands(
    const std::vector<double> & displacements = {0.45},
    const std::vector<double> & velocities = {0.0}, const double duration = 1.25)
  {
    auto wait_for_topic = [&](const auto topic_name)
    {
      size_t wait_count = 0;
      while (command_publisher_node_->count_subscribers(topic_name) == 0)
      {
        if (wait_count >= 5)
        {
          auto error_msg =
            std::string("publishing to ") + topic_name + " but no node subscribes to it";
          throw std::runtime_error(error_msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++wait_count;
      }
    };

    wait_for_topic(command_publisher_->get_topic_name());

    ControllerCommandMsg msg;
    msg.index = 0u;
    msg.subindex = 0u;
    msg.data = 0u;

    command_publisher_->publish(msg);
  }

protected:
  // Controller-related parameters
  std::string joint_name_ = {"joint1"};
  std::vector<std::string> command_interface_names_ = {
    "tpdo/index", "tpdo/subindex", "tpdo/data", "tpdo/ons",
    "nmt/reset",  "nmt/reset_fbk", "nmt/start", "nmt/start_fbk"};

  std::vector<std::string> state_interface_names_ = {
    "rpdo/index", "rpdo/subindex", "rpdo/data", "nmt/state"};

  // see StateInterfaces in canopen_proxy_controller.hpp for correct order;
  std::array<double, 4> joint_state_values_ = {0, 0, 0, 0};
  // see CommandInterfaces in canopen_proxy_controller.hpp for correct order;
  std::array<double, 8> joint_command_values_ = {101.101, 101.101, 101.101, 101.101,
                                                 101.101, 101.101, 101.101, 101.101};

  std::vector<hardware_interface::StateInterface> state_itfs_;
  std::vector<hardware_interface::CommandInterface> command_itfs_;

  // Test related parameters
  std::unique_ptr<TestableCanopenProxyController> controller_;
  rclcpp::Node::SharedPtr command_publisher_node_;
  rclcpp::Publisher<ControllerCommandMsg>::SharedPtr command_publisher_;
  rclcpp::Node::SharedPtr service_caller_node_;
  rclcpp::Client<ControllerModeSrvType>::SharedPtr slow_control_service_client_;
};

// From the tutorial: https://www.sandordargo.com/blog/2019/04/24/parameterized-testing-with-gtest
class CanopenProxyControllerTestParameterizedParameters
: public CanopenProxyControllerFixture<TestableCanopenProxyController>,
  public ::testing::WithParamInterface<std::tuple<std::string, rclcpp::ParameterValue>>
{
public:
  virtual void SetUp() { CanopenProxyControllerFixture::SetUp(); }

  static void TearDownTestCase() { CanopenProxyControllerFixture::TearDownTestCase(); }

protected:
  void SetUpController(bool set_parameters = true)
  {
    CanopenProxyControllerFixture::SetUpController(set_parameters);
    controller_->get_node()->set_parameter({std::get<0>(GetParam()), std::get<1>(GetParam())});
  }
};

#endif  // CANOPEN_ROS2_CONTROLLERS__CONTROLLER__TEST_CANOPEN_PROXY_CONTROLLER_HPP_
