#ifndef STOPWATCH_H
#define STOPWATCH_H
/**
* stopwatch functions to measure cpu process time
* reference:
*https://github.com/stonier/ecl_core/blob/release/0.61-indigo-kinetic/ecl_time/include/ecl/time/stopwatch.hpp
*
**/

#include <ros/time.h>
#include <queue>
#include <string>
#include <vector>

#include <local_planner/Profiling.h>

#include <local_planner/LocalPlannerNodeConfig.h>

namespace avoidance {
/**
 * @brief A timepiece that emulates the functionality of a stopwatch.
 *
 * This uses the TimeStamp class as a means of recording time, splits and
 * elapsed time. Its operation should be intuitive.
**/

class StopWatch {
 public:
  // store all durations of a function
  ros::Duration total_duration_;
  ros::Duration duration_e_;
  ros::Duration duration_s_;
  int counter_ = 0;
  std::string function_name_;

  ros::Publisher duration_measurement_pub_;
  /**
   * Default constructor that initialises the stopwatch with the current
   * system time to use as a point of reference.
   **/
  StopWatch();

  virtual ~StopWatch(){};

  /**
  * @brief restarts the stopwatch
  **/
  void restart();

  /**
  * @brief Calculates the total elapsed time
  * @return ros::Duration: the total elapsed time since (re)started.
  **/
  ros::Duration elapsed();
  /**
   * @brief Calculates the current split.
   *
   * Calculates the elapsed time since the last split.
   * @return ros::Duration : the elapsed time since the last split.
   **/
  ros::Duration split();
  /**
   * @brief Create ProcessTIme message
   **/
  void setProcessTimeMsg(local_planner::Profiling& msg,
                         const std::string& frame_id);

 private:
  ros::NodeHandle timing_node_;
  ros::Time start_time_, split_time_;
};
void setProfilingMsg(local_planner::Profiling& msg, const std::string& frame_id,
                     const std::string& function_name, ros::Duration duration,
                     const int counter);
}
#endif  // STOPWATCH_H