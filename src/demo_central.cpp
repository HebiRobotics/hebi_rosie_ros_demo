#include <memory>

#include <ros/ros.h>
#include <ros/console.h>

#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>

#include <hebi_cpp_api_examples/ArmMotionAction.h>
#include <hebi_cpp_api_examples/BaseMotionAction.h>
#include <hebi_rosie_demo/GripperSrv.h>
#include <hebi_rosie_demo/VisionSrv.h>
#include <hebi_rosie_demo/CalibrateSrv.h>

#include "Eigen/Core"
#include <Eigen/SVD>

#include "group.hpp"
#include "group_feedback.hpp"
#include "lookup.hpp"


// We abstract the behavior of each of the core components up here, so our
// main logic loop doesn't have to deal with ros services, actions, etc.

// TODO: REMOVE THIS? (use hebi::Color instead?)
struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

class Gripper {
public:
  Gripper(ros::NodeHandle& node)
    : client_(node.serviceClient<hebi_rosie_demo::GripperSrv>("rosie/gripper")) {
  }

  bool close() {
    message_.request.closed = true;
    if (client_.call(message_)) {
      ros::Duration(0.75).sleep();
      return true;
    }
    return false;
  }

  bool open() {
    message_.request.closed = false;
    if (client_.call(message_)) {
      ros::Duration(1).sleep();
      return true;
    }
    return false;
  }

private:
  ros::ServiceClient client_;
  hebi_rosie_demo::GripperSrv message_;
};

class Arm {
public:
  struct Location {
    double x;
    double y;
    double z;
  };

  Arm(ros::NodeHandle& node)
    : gripper_(node), arm_motion_("rosie/arm/motion", true) {
    // Wait for the server to start (can wait forever)
    arm_motion_.waitForServer();
  }

  bool canReach(const Location& location) {
    bool can_reach = 
      location.x > 0.25 && location.x < 0.45 &&
      location.y > -0.20 && location.y < 0.20 &&
      location.z > -0.11 && location.z < -0.09;
    if (can_reach)
      ROS_INFO("Can reach %f %f %f", location.x, location.y, location.z);
    else
      ROS_INFO("Cannot reach %f %f %f", location.x, location.y, location.z);
    return can_reach;
//    return false;
  }

  bool moveHome() {
    setGoalIntermediate();
    setGoalHome();
    if (!moveToGoal())
      return false;
  }

  bool pickup(const Location& location, const Color& color) {
    setColor(color);
    // Call service to move to pick up location, facing down:
    clearLocations();
    setGoalIntermediate();
    setGoalLocation(location, true);
    if (!moveToGoal()) {
      clearColor();
      return false;
    }

    // Pick up with gripper
    if (!gripper_.close()) {
      clearColor();
      return false;
    }

    // TODO: smooth these motions into a single action! Need additional functionality from RosieArmNode...
    // Call service to move home and then move to drop position
    clearLocations();
    setGoalIntermediate();
    setGoalDrop();
    if (!moveToGoal()) {
      clearColor();
      return false;
    }

    if (!gripper_.open()) {
      clearColor();
      return false;
    }

    // Call service to move home
    clearLocations();
    setGoalIntermediate();
    setGoalHome();
    if (!moveToGoal()) {
      clearColor();
      return false;
    }

    return true;
  }

  bool deployBags() {
    clearColor();
    clearLocations();
    setGoalIntermediate();
    setGoalDrop();
    setGoalBox();
    if (!moveToGoal())
      return false;

    // Pick up with gripper
    if (!gripper_.close())
      return false;

    clearLocations();
    setGoalDrop();
    setGoalIntermediate();
    setGoalThrow();
    if (!moveToGoal())
      return false;

    if (!gripper_.open())
      return false;

    return true;
    
  }

private:

  // TODO: could refactor so this takes in argument, and "setGoal" etc. functions
  // below are just static consts instead...
  bool moveToGoal() {
    arm_motion_.sendGoal(arm_motion_goal_);

    // wait for the action to return
    bool finished_before_timeout = arm_motion_.waitForResult(ros::Duration(15.0));

    if (!finished_before_timeout) {
      ROS_ERROR("Arm motion action timed out");
      return false;
    }

    auto res = arm_motion_.getState();
    ROS_INFO("Arm motion action finished: %s", res.toString().c_str());
    return true;
  }

  void clearLocations() {
    arm_motion_goal_.x.clear();
    arm_motion_goal_.y.clear();
    arm_motion_goal_.z.clear();
    arm_motion_goal_.tipx.clear();
    arm_motion_goal_.tipy.clear();
    arm_motion_goal_.tipz.clear();
  }

  // If not down, assume forward:
  void setGoalLocation(const Location& location, bool is_down) {
    arm_motion_goal_.x.push_back(location.x);
    arm_motion_goal_.y.push_back(location.y);
    arm_motion_goal_.z.push_back(location.z);
    if (is_down) {
      arm_motion_goal_.tipx.push_back(0);
      arm_motion_goal_.tipy.push_back(0);
      arm_motion_goal_.tipz.push_back(-1);
    } else {
      arm_motion_goal_.tipx.push_back(1);
      arm_motion_goal_.tipy.push_back(0);
      arm_motion_goal_.tipz.push_back(0);
    }
  }

  void setGoalHome() {
    setGoalLocation({0.2, -0.2, 0.3}, true);
  }

  void setGoalThrow() {
    setGoalLocation({0.3, -0.3, 0.3}, true);
  }

  void setGoalDrop() {
//    setGoalLocation({-0.09, -0.18, 0.3}, true);
//    setGoalLocation({+0.35, +0.35, 0.3}, true);
    setGoalLocation({-0.02, +0.2, 0.3}, true);
  }

  void setGoalBox() {
//    setGoalLocation({-0.09, -0.18, -0.04}, true);
//    setGoalLocation({+0.35, +0.35, +0.04}, true);
    setGoalLocation({-0.02, +0.2, 0.1}, true);
  }

  void setGoalIntermediate() {
    setGoalLocation({0.35, +0.1, 0.3}, true);
  }

  void setColor(const Color& c) {
    arm_motion_goal_.set_color = true;
    arm_motion_goal_.r = c.r;
    arm_motion_goal_.g = c.g;
    arm_motion_goal_.b = c.b;
  }

  void clearColor() {
    arm_motion_goal_.set_color = false;
  }

  Gripper gripper_;

  // true causes the client to spin its own thread
  actionlib::SimpleActionClient<hebi_cpp_api_examples::ArmMotionAction> arm_motion_;

  hebi_cpp_api_examples::ArmMotionGoal arm_motion_goal_;
};

class Base {
public:
  struct Location {
    double x;
    double y;
  };

  Base(ros::NodeHandle& node)
    : base_motion_("rosie/base/motion", true) {
    // Wait for the server to start (can wait forever)
    base_motion_.waitForServer();
  }

  bool rotate(double radians, const Color* color) {
    if (color) {
      base_motion_goal_.set_color = true;
      base_motion_goal_.r = color->r;
      base_motion_goal_.g = color->g;
      base_motion_goal_.b = color->b;
    } else {
      base_motion_goal_.set_color = false;
    }
    
    base_motion_goal_.x = 0.0;
    base_motion_goal_.y = 0.0;
    base_motion_goal_.theta = radians; 

    base_motion_.sendGoal(base_motion_goal_);

    // wait for the action to return
    bool finished_before_timeout = base_motion_.waitForResult(ros::Duration(30.0));

    if (!finished_before_timeout) {
      ROS_ERROR("Base motion action timed out");
      return false;
    }

    auto res = base_motion_.getState();
    ROS_INFO("Base motion action finished: %s", res.toString().c_str());
    return true;
  }

  bool moveTo(const Location& location, const Color* color) {

    if (color) {
      base_motion_goal_.set_color = true;
      base_motion_goal_.r = color->r;
      base_motion_goal_.g = color->g;
      base_motion_goal_.b = color->b;
    } else {
      base_motion_goal_.set_color = false;
    }
    
    // Logic here to not just run over the friggin' thing
    base_motion_goal_.theta = atan2(location.y, location.x);
    double len = std::sqrt(location.x * location.x + location.y * location.y);
    double actual_len = len - 0.275; // stay [m] away from the robot center.
//    if (actual_len < 0)
//      actual_len = 0;
    double frac = actual_len / len;
    base_motion_goal_.x = location.x * frac;
    base_motion_goal_.y = location.y * frac;
    ROS_INFO("Base motion moving: %f %f %f", base_motion_goal_.x, base_motion_goal_.y, base_motion_goal_.theta);

    base_motion_.sendGoal(base_motion_goal_);

    // wait for the action to return
    bool finished_before_timeout = base_motion_.waitForResult(ros::Duration(30.0));

    if (!finished_before_timeout) {
      ROS_ERROR("Base motion action timed out");
      return false;
    }

    auto res = base_motion_.getState();
    ROS_INFO("Base motion action finished: %s", res.toString().c_str());
    return true;
  }

  // true causes the client to spin its own thread
  actionlib::SimpleActionClient<hebi_cpp_api_examples::BaseMotionAction> base_motion_;

  hebi_cpp_api_examples::BaseMotionGoal base_motion_goal_;

  // TODO: ADD SET COLOR SERVICE HERE!!!
};

class AffineTransform {
public:
  AffineTransform() {
    trans_.setZero();
  }
    
  bool trySetFromROSParam(const std::vector<double>& params) {
    if (params.size() != 6) {
      ROS_ERROR("Calibration matrix parameter is invalid size! Should be 6 elements!");
      return false;
    }
    trans_(0, 0) = params[0];
    trans_(0, 1) = params[1];
    trans_(0, 2) = params[2];
    trans_(1, 0) = params[3];
    trans_(1, 1) = params[4];
    trans_(1, 2) = params[5];
    return true;
  }

  void writeForROSParam(std::vector<double>& params) {
    params.resize(6);
    params[0] = trans_(0, 0);
    params[1] = trans_(0, 1);
    params[2] = trans_(0, 2);
    params[3] = trans_(1, 0);
    params[4] = trans_(1, 1);
    params[5] = trans_(1, 2);
  }

  void setTransform(const Eigen::Matrix<double, 2, 3>& new_trans) {
    trans_ = new_trans;
  }

  void transform(double& x, double& y) {
    double tmp_x = x * trans_(0, 0) + y * trans_(0, 1) + trans_(0, 2);
    y =            x * trans_(1, 0) + y * trans_(1, 1) + trans_(1, 2);
    x = tmp_x;
  }

  std::string toString() {
    std::stringstream res;
    res << "rosie/calibration: [" <<
      trans_(0, 0) << ", " << trans_(0, 1) << ", " << trans_(0, 2) << ", " <<
      trans_(1, 0) << ", " << trans_(1, 1) << ", " << trans_(1, 2) << "]";
    return res.str();
  }

private:
  Eigen::Matrix<double, 2, 3> trans_;
};

AffineTransform calibrate_trans;

class Vision {
public:
  struct Location {
    double x;
    double y;
  };

  Vision(ros::NodeHandle& node)
    : client_(node.serviceClient<hebi_rosie_demo::VisionSrv>("rosie/vision")),
      calibrate_client_(node.serviceClient<hebi_rosie_demo::CalibrateSrv>("rosie/calibrate_vision")) {
  }

  bool search(Location& found_at, Color& found_color) {
    // Got a success back?
    if (client_.call(message_) && message_.response.found) {
      found_at.x = message_.response.x;
      found_at.y = message_.response.y;
      found_color.r = message_.response.r;
      found_color.g = message_.response.g;
      found_color.b = message_.response.b;
      return true;
    }
    return false;
  }

  // Collect:
  //   a: 3xn matrix of points in camera frame, with row of '1' at the bottom
  //   b: 3xn matrix of points in world frame (in meters) that correspond, with row of '1' at the bottom.
  // Compute this in MATLAB (basically, using least squares solution):
  //   X = b / a
  // or equivalently
  //   X = a' \ b'
  bool calibrate() {
    ROS_WARN("CALIBRATION STARTED");
    if (calibrate_client_.call(calibrate_message_) && calibrate_message_.response.found) {

      if (calibrate_message_.response.points.size() != 30)
        return false;

      // Go through all points, do SVD!
      int last_pt = 29; // 30 points, 0 indexed
      // The 30 points should be col-by-col, but L/R and U/D ordering may differ.

      // First and last col average x values:
      float avg_x_left = 0;
      float avg_x_right = 0;
      for (size_t i = 0; i < 5; ++i) {
        avg_x_left += calibrate_message_.response.points[i].x;
        avg_x_right += calibrate_message_.response.points[last_pt - i].x;
      }
      // The world point we hardcode go from positive to negative x:
      bool flip_x = avg_x_left < avg_x_right;
      // This means [0] <-> [last_pt - 4], [1] <-> [last_pt - 3], ... [4] <-> [last_pt]

      // First and last row average y values:
      float avg_y_top = 0;
      float avg_y_bot = 0;
      for (size_t i = 0; i < 6; ++i) {
        avg_y_top += calibrate_message_.response.points[i * 5].y;
        avg_y_bot += calibrate_message_.response.points[last_pt - i * 5].y;
      }
      // The world point we hardcode go from far away (top) to close (bottom):
      bool flip_y = avg_y_top > avg_y_bot;
      // This means [0] <-> [4], [1] <-> [3], ... [last_pt - 4] <-> [last_pt]

      Eigen::MatrixXd camera_pts(3, 30);
      size_t rows = 5;
      size_t cols = 6;
      for (size_t row = 0; row < rows; ++row) {
        int source_row = flip_y ? (rows - 1 - row) : row;
        for (size_t col = 0; col < cols; ++col) {
          int source_col = flip_x ? (cols - 1 - col) : col;
          int dest_i = row + col * rows;
          int source_i = source_row + source_col * rows;
          
          camera_pts(0, dest_i) = calibrate_message_.response.points[source_i].x;
          camera_pts(1, dest_i) = calibrate_message_.response.points[source_i].y;
          camera_pts(2, dest_i) = 1.0;
        }
      }

      // The points they should match up with:
      Eigen::MatrixXd world_pts(3, 30);
      double spacing = 0.075; // mm between row centers
      double top_row = 0.15 + 0.56; // 15 cm from center of arm to front of robot + 54cm (Letter) / 56cm (A4) to top row on paper.
      double right_col = -2.5 * spacing; // 2.5 rows to the right
      for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 6; ++col) {
          world_pts(0, col * 5 + row) = top_row - (double)row * spacing;
          world_pts(1, col * 5 + row) = right_col + (double)col * spacing;
          world_pts(2, col * 5 + row) = 1.0;
        }
      }

      Eigen::JacobiSVD<Eigen::MatrixXd> svd(camera_pts.transpose(), Eigen::ComputeThinU | Eigen::ComputeThinV);
     
      Eigen::MatrixXd calibrate_matrix = svd.solve(world_pts.transpose());
      ROS_WARN("CALIBRATION SUCCESS");
      // Calibration debugging printouts, if needed:
      //ROS_INFO("Size %d %d",
      //  (int)calibrate_matrix.rows(), (int)calibrate_matrix.cols());
      //ROS_INFO("My Matrix! %f %f %f %f %f %f %f %f %f",
      //  calibrate_matrix(0, 0), calibrate_matrix(0, 1), calibrate_matrix(0, 2),
      //  calibrate_matrix(1, 0), calibrate_matrix(1, 1), calibrate_matrix(1, 2),
      //  calibrate_matrix(2, 0), calibrate_matrix(2, 1), calibrate_matrix(2, 2));
      //ROS_INFO_STREAM("Camera\n" << camera_pts);
      //ROS_INFO_STREAM("World\n" << world_pts);

      Eigen::Matrix<double, 2, 3> tmp;
      for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 3; ++j)
          tmp(i, j) = calibrate_matrix(j, i);
      calibrate_trans.setTransform(tmp);

      return true;
    }
    ROS_WARN("FAILED CALIBRATION");
    return false;
  }

private:
  ros::ServiceClient client_;
  hebi_rosie_demo::VisionSrv message_;
  ros::ServiceClient calibrate_client_;
  hebi_rosie_demo::CalibrateSrv calibrate_message_;
};

// Note -- could do this in a super fancy class template abstract/general way
// with a "LocationTransformer" static template class...
// for now, we just hardcode transformations from Vision to Arm and base here...
Arm::Location transformToArm(const Vision::Location& source)
{
  double x = source.x;
  double y = source.y;
  calibrate_trans.transform(x, y);
  double yMountingOffset = -0.045; // robot is mounted 4.5cm towards right of center (negative y)
  return Arm::Location{x, y - yMountingOffset, -0.1};
}

Base::Location transformToBase(const Vision::Location& source)
{
  double x = source.x;
  double y = source.y;
  calibrate_trans.transform(x, y);
  // The base axis for the wheels is 7.5 cm in front of the arm base (and vision coordinate system)
  return Base::Location{x - 0.075, y};
}

// Direct (non-ROS) connection to the iPad.
class IPad {
public:
  static std::unique_ptr<IPad> create(const std::string& family, const std::string& name) {
    hebi::Lookup lookup;
    std::shared_ptr<hebi::Group> group;
    while (!group) {
      ROS_INFO("Looking for iPad");
      group = lookup.getGroupFromNames({ family }, { name });
    }
    ROS_INFO("iPad found!");
    return std::unique_ptr<IPad>(new IPad(group));
  }

  enum class Mode {
    Pause,
    Calibrate,
    Drive,
    Deploy,
    Autonomous,
    Quit
  };

  struct State {
    Mode to_mode{Mode::Quit}; // Placeholder! Initialize to quit, and wait until "pause" to start.
    float drive_forward{0}; // [-1 to 1]
    float drive_left{0}; // [-1 to 1]
  };

  State& getState() {
    return _state;
  }

private:
  IPad(std::shared_ptr<hebi::Group> group) : _group(group)
  {
    // Since we are polling, let's do this a little more quickly!
    _group->setFeedbackFrequencyHz(250);
    _group->addFeedbackHandler([this] (const hebi::GroupFeedback& feedback) {
      auto& fbk = feedback[0];
      if (fbk.io().b().hasInt(1) && fbk.io().b().getInt(1) == 1) {
        _state.to_mode = Mode::Pause;
      }
      if (fbk.io().b().hasInt(2) && fbk.io().b().getInt(2) == 1) {
        _state.to_mode = Mode::Autonomous;
      }
      if (fbk.io().b().hasInt(3) && fbk.io().b().getInt(3) == 1) {
        _state.to_mode = Mode::Calibrate;
      }
      if (fbk.io().b().hasInt(4) && fbk.io().b().getInt(4) == 1) {
        _state.to_mode = Mode::Deploy;
      }
      // TODO: Drive
      if (fbk.io().b().hasInt(5) && fbk.io().b().getInt(5) == 1) {
        _state.to_mode = Mode::Quit;
      }
    });
  }

  std::shared_ptr<hebi::Group> _group;
  State _state;
};

int main(int argc, char ** argv) {

  ros::init(argc, argv, "demo_central");

  ros::NodeHandle node;

  // Get calibration from parameter server:
  if (!node.hasParam("rosie/calibration")) {
    ROS_ERROR("Could not find rosie/calibration on parameter server! Exiting!");
    return -1;
  }

  std::vector<double> calibration_param;
  bool parsed_param = node.getParam("rosie/calibration", calibration_param) ? calibrate_trans.trySetFromROSParam(calibration_param) : false;
  if (!parsed_param) {
    ROS_ERROR("Could not parse rosie/calibration from parameter server! Exiting!");
    return -1;
  }

  constexpr double command_rate = 100; // Hz
  ros::Rate loop_rate(command_rate);

  constexpr double rotate_increment = M_PI / 3.0; // 1/6 of a full rotation

  // Initialize abstracted components and their ROS interfaces
  auto ipad = IPad::create("HEBI", "Mobile IO"); // This blocks forever...
  Base base(node);
  Vision vision(node);
  Vision::Location location;
  Color color;
  IPad::State& state = ipad->getState(); // Note -- this updates in the background!

  // Wait for iPad start before continuing!
  while (ros::ok())
  {
    if (state.to_mode == IPad::Mode::Pause)
      break;
  }
  Arm arm(node);
  arm.moveHome();

  // TODO: load calibration!

  ros::Duration pause_wait(1);
  bool calibrate_latch = false;

  int spin_count = 0;
  int arm_count = 0;

  // Run main logic
  while (ros::ok()) {
    if (state.to_mode == IPad::Mode::Autonomous)
    {
      calibrate_latch = false; // reset latch
      // Can't find anything? rotate and continue the search
      if (!vision.search(location, color)) {
        base.rotate(rotate_increment, nullptr);
        ++spin_count;
        if (spin_count >= 6) {
          state.to_mode = IPad::Mode::Deploy;
          arm_count = 0;
          spin_count = 0;
        } 
        continue;
      }
      spin_count = 0;
      if (state.to_mode == IPad::Mode::Pause)
        continue;

      // Can the arm reach this? If so, retrieve, then continue the search
      auto arm_location = transformToArm(location);
      if (arm.canReach(arm_location)) {
        arm.pickup(arm_location, color);
        continue;
      }
      if (state.to_mode == IPad::Mode::Pause)
        continue;

      // Otherwise, go there with the base and then continue the search
      base.moveTo(transformToBase(location), &color);
    } else if (state.to_mode == IPad::Mode::Pause) {
      arm_count = 0;
      spin_count = 0;
      calibrate_latch = false; // reset latch
      pause_wait.sleep();
    } else if (state.to_mode == IPad::Mode::Calibrate && !calibrate_latch) {
      arm_count = 0;
      spin_count = 0;
      if (vision.calibrate()) {
        // Update the parameter on the parameter server:
        calibrate_trans.writeForROSParam(calibration_param);
        node.setParam("rosie/calibration", calibration_param);
        ROS_WARN_STREAM("New calibration parameter: " << calibrate_trans.toString());
        ROS_WARN_STREAM("These parameters are active for the remainder of program execution, but must be manually set in parameters/calibration.txt to affect future launches.");
      }

      calibrate_latch = true; // don't re-calibrate once we've done it once!
    } else if (state.to_mode == IPad::Mode::Drive) {
      arm_count = 0;
      spin_count = 0;
      calibrate_latch = false; // reset latch
      // TODO: drive
    } else if (state.to_mode == IPad::Mode::Deploy) {
      calibrate_latch = false; // reset latch
      arm.deployBags();
      ++arm_count;
      if (arm_count >= 3) {
        base.moveTo(
          Base::Location{0.65, 0.25}, nullptr);
        state.to_mode = IPad::Mode::Autonomous;
        arm_count = 0;
        spin_count = 0;
      } 
      base.rotate(rotate_increment, nullptr);
    } else if (state.to_mode == IPad::Mode::Quit) {
      break;
    }

    ros::spinOnce();
  }

  return 0;
}

