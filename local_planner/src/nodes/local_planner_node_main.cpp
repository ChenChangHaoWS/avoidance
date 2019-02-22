#include "local_planner/local_planner.h"
#include "local_planner/local_planner_node.h"
#include "local_planner/waypoint_generator.h"
#include "local_planner/stopwatch.h"

// in rovio the custom msg was in <>
#include <local_planner/ProcessTime.h>

#include <boost/algorithm/string.hpp>


int main(int argc, char** argv) {
  using namespace avoidance;
  ros::init(argc, argv, "local_planner_node");
  LocalPlannerNode Node;
  ros::Duration(2).sleep();
  ros::Time start_time = ros::Time::now();
  bool hover = false;
  bool landing = false;
  avoidanceOutput planner_output;
  Node.local_planner_->disable_rise_to_goal_altitude_ =
      Node.disable_rise_to_goal_altitude_;
  bool startup = true;
  bool callPx4Params = true;
  Node.status_msg_.state = (int)MAV_STATE::MAV_STATE_BOOT;

  std::thread worker(&LocalPlannerNode::threadFunction, &Node);
  
  // add stuff to stop time
  StopWatch stopwatch1;
  StopWatch stopwatch2;

  // spin node, execute callbacks
  while (ros::ok()) {
    hover = false;

    // visualize world in RVIZ
    if (!Node.world_path_.empty() && startup) {
      visualization_msgs::MarkerArray marker_array;
      if (!visualizeRVIZWorld(Node.world_path_, marker_array)) {
        Node.world_pub_.publish(marker_array);
      }
      startup = false;
    }

    // Process callbacks & wait for a position update
    while (!Node.position_received_ && ros::ok()) {
      ros::getGlobalCallbackQueue()->callAvailable(ros::WallDuration(0.1));
    }

    // Check if all information was received
    ros::Time now = ros::Time::now();
    ros::Duration pointcloud_timeout_land =
        ros::Duration(Node.local_planner_->pointcloud_timeout_land_);
    ros::Duration pointcloud_timeout_hover =
        ros::Duration(Node.local_planner_->pointcloud_timeout_hover_);
    ros::Duration since_last_cloud = now - Node.last_wp_time_;
    ros::Duration since_start = now - start_time;
    std::cout << "since start " << since_start << std::endl;

    if (since_last_cloud > pointcloud_timeout_land &&
        since_start > pointcloud_timeout_land) {
      if (!landing) {
        mavros_msgs::SetMode mode_msg;
        mode_msg.request.custom_mode = "AUTO.LOITER";
        landing = true;
        Node.status_msg_.state = (int)MAV_STATE::MAV_STATE_FLIGHT_TERMINATION;
        if (Node.mavros_set_mode_client_.call(mode_msg) &&
            mode_msg.response.mode_sent) {
          ROS_WARN("\033[1;33m Pointcloud timeout: Landing \n \033[0m");
        } else {
          ROS_ERROR(
              "\033[1;33m Pointcloud timeout: Landing failed! \n \033[0m");
        }
      }
    } else {
      if (Node.never_run_ || (since_last_cloud > pointcloud_timeout_hover &&
                              since_start > pointcloud_timeout_hover)) {
        if (Node.position_received_) {
          hover = true;
          Node.status_msg_.state = (int)MAV_STATE::MAV_STATE_CRITICAL;
          std::string not_received = "";
          for (size_t i = 0; i < Node.cameras_.size(); i++) {
            if (!Node.cameras_[i].received_) {
              not_received.append(" , no cloud received on topic ");
              not_received.append(Node.cameras_[i].topic_);
            }
          }
          if (!Node.canUpdatePlannerInfo()) {
            not_received.append(" , missing transforms ");
          }
          ROS_INFO(
              "\033[1;33m Pointcloud timeout %s (Hovering at current position) "
              "\n "
              "\033[0m",
              not_received.c_str());
        } else {
          ROS_WARN(
              "\033[1;33m Pointcloud timeout: No position received, no WP to "
              "output.... \n \033[0m");
        }
      }
    }

    // If planner is not running, update planner info and get last results
    if (Node.cameras_.size() == Node.numReceivedClouds() &&
        Node.cameras_.size() != 0) {
      if (Node.canUpdatePlannerInfo()) {
        if (Node.running_mutex_.try_lock()) {
          //todo: insert start time updatePlannerInfo_time
          local_planner::ProcessTime updatePlannerInfo_msg;
          std::string frame_id = "/local_planner_node_main";
          stopwatch2.function_name_ = "updatePlannerInfo";
          //uint64_t updatePlannerInfo_time = stopwatch1.split();
          stopwatch2.restart();
          
          Node.updatePlannerInfo();
          //stopwatch1.counter_++;
          stopwatch2.counter_++;
          //updatePlannerInfo_time = stopwatch1.elapsed();
          ros::Duration duration = stopwatch2.elapsed();

          //stopwatch2.timings_.push_back(duration); 
          stopwatch2.setProcessTimeMsg(updatePlannerInfo_msg, frame_id);
          //std::cout << "updatePlannerInfo1 took " << updatePlannerInfo_time  << std::endl;
          std::cout << "updatePlannerInfo2 took " << duration  << std::endl;
          Node.duration_measurement_pub_.publish(updatePlannerInfo_msg);
          // reset all clouds to not yet received
          for (size_t i = 0; i < Node.cameras_.size(); i++) {
            Node.cameras_[i].received_ = false;
          }
          // check how long the wp generator update takes
          stopwatch1.function_name_ = "setPlannerInfo";
          stopwatch1.restart();
          Node.wp_generator_->setPlannerInfo(
            // checkout how long it takes to get the avoidance output
              Node.local_planner_->getAvoidanceOutput()
              // end avoidance output
              );
          stopwatch1.counter_++;
          //updatePlannerInfo_time = stopwatch1.elapsed();
          ros::Duration duration1 = stopwatch1.elapsed();

          //stopwatch2.timings_.push_back(duration); 
          stopwatch1.setProcessTimeMsg(updatePlannerInfo_msg, frame_id);
          //std::cout << "updatePlannerInfo1 took " << updatePlannerInfo_time  << std::endl;
          std::cout << "updatePlannerInfo2 took " << duration1  << std::endl;
          Node.duration_measurement_pub_.publish(updatePlannerInfo_msg);
          // end wp generator
          if (Node.local_planner_->stop_in_front_active_) {
            Node.goal_msg_.pose.position = Node.local_planner_->getGoal();
          }
          Node.running_mutex_.unlock();
          // Wake up the planner
          std::unique_lock<std::mutex> lck(Node.data_ready_mutex_);
          Node.data_ready_ = true;
          Node.data_ready_cv_.notify_one();
        }
      }
    }

    // send waypoint
    if (!Node.never_run_ && !landing) {
      Node.publishWaypoints(hover);
      if (!hover) Node.status_msg_.state = (int)MAV_STATE::MAV_STATE_ACTIVE;
    } else {
      for (size_t i = 0; i < Node.cameras_.size(); ++i) {
        // once the camera info have been set once, unsubscribe from topic
        Node.cameras_[i].camera_info_sub_.shutdown();
      }
    }

    Node.position_received_ = false;

    // publish system status
    if (now - Node.t_status_sent_ > ros::Duration(0.2)) {
      Node.status_msg_.header.stamp = ros::Time::now();
      Node.status_msg_.component = 196;  // MAV_COMPONENT_ID_AVOIDANCE
      Node.mavros_system_status_pub_.publish(Node.status_msg_);
      Node.t_status_sent_ = now;
    }
  }


  Node.should_exit_ = true;
  Node.data_ready_cv_.notify_all();
  worker.join();
  return 0;
}
