#include "local_planner/stopwatch.h"

#include <ros/console.h>

namespace avoidance {

StopWatch::StopWatch() {
  timing_node_ = ros::NodeHandle("~");
  duration_measurement_pub_ =
      timing_node_.advertise<local_planner::Profiling>("/performance_check", 1);
}

void StopWatch::restart() {
  start_time_ = ros::Time::now();
  split_time_ = start_time_;
  std::cout << "stopwatch start " << start_time_ << std::endl;
}

ros::Duration StopWatch::elapsed() {
  ros::Time current_time = ros::Time::now();
  duration_e_ = current_time - start_time_;
  std::cout << "stopwatch current " << current_time << " duration "
            << duration_e_ << std::endl;
  return duration_e_;
}

ros::Duration StopWatch::split() {
  ros::Time last_time = split_time_;
  std::cout << "stopwatch2 last " << last_time << " split time " << split_time_
            << std::endl;
  split_time_ = ros::Time::now();
  std::cout << "stopwatch2 last " << last_time << " split time " << split_time_
            << std::endl;
  duration_s_ = split_time_ - last_time;
  total_duration_ += duration_s_;
  return duration_s_;
}

void StopWatch::setProcessTimeMsg(local_planner::Profiling& msg,
                                  const std::string& frame_id) {
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = frame_id;
  msg.function_name = function_name_;
  msg.duration = duration_e_;
  msg.counter = counter_;
}

void setProfilingMsg(local_planner::Profiling& msg, const std::string& frame_id,
                     const std::string& function_name, ros::Duration duration,
                     const int counter) {
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = frame_id;
  msg.function_name = function_name;
  msg.duration = duration;
  msg.counter = counter;
}
}
