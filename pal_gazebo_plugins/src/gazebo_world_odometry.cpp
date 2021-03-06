#include <assert.h>
#include <pal_gazebo_plugins/gazebo_world_odometry.h>
#include <ignition/math.hh>
#include <sdf/sdf.hh>
#include <ros/ros.h>
#include <cmath>
#include <nav_msgs/Odometry.h>

namespace gazebo {

  void threeaxisrot(double r11, double r12, double r21, double r31, double r32, double res[]){
    res[0] = atan2( r31, r32 );
    res[1] = asin ( r21 );
    res[2] = atan2( r11, r12 );
  }

  GazeboWorldOdometry::GazeboWorldOdometry(){}

  // Destructor
  GazeboWorldOdometry::~GazeboWorldOdometry(){
    this->rosNode_->shutdown();
  }

  // Load the controller
  void GazeboWorldOdometry::Load(physics::ModelPtr parent_model, sdf::ElementPtr _sdf){

    ROS_INFO_STREAM("Loading gazebo WORLD ODOMETRY RPY plugin");

    this->robot_namespace_ = "";
    if (_sdf->HasElement("robotNamespace"))
      this->robot_namespace_ =
        _sdf->GetElement("robotNamespace")->Get<std::string>() + "/";

    this->topic_name_ = "ft_data";
    if (_sdf->GetElement("topicName"))
      this->topic_name_ =
        _sdf->GetElement("topicName")->Get<std::string>();

    if (!_sdf->HasElement("frameName"))
    {
      ROS_INFO("world odometry sensor plugin missing <frameName>, defaults to world");
      this->frame_name_ = "world";
    }
    else
      this->frame_name_ = _sdf->GetElement("frameName")->Get<std::string>();

    this->world_ = parent_model->GetWorld();
    std::string link_name_ = frame_name_;
    // assert that the body by link_name_ exists
    this->link = boost::dynamic_pointer_cast<gazebo::physics::Link>(
      this->world_->EntityByName(link_name_));
    if (!this->link)
    {
      ROS_FATAL("gazebo_ros_world_ogometry plugin error: bodyName: %s does not exist\n",
        link_name_.c_str());
    }

    this->update_rate_ = 1000.0;
    if (!_sdf->HasElement("updateRate"))
    {
      ROS_INFO("world odometry plugin missing <updateRate>, defaults to %f", this->update_rate_);
    }
    else
      this->update_rate_ = _sdf->GetElement("updateRate")->Get<double>();

    // ros callback queue for processing subscription
    this->deferredLoadThread = boost::thread(
      boost::bind(&GazeboWorldOdometry::DeferredLoad, this));
  }

  ////////////////////////////////////////////////////////////////////////////////
  void GazeboWorldOdometry::DeferredLoad(){

    // Make sure the ROS node for Gazebo has already been initialized
    if (!ros::isInitialized())
    {
      ROS_FATAL_STREAM("A ROS node for Gazebo has not been initialized, unable to load plugin. "
        << "Load the Gazebo system plugin 'libgazebo_ros_api_plugin.so' in the gazebo_ros package)");
      return;
    }

    this->rosNode_ = new ros::NodeHandle(this->robot_namespace_);
    floatingBasePub_ = this->rosNode_->advertise<nav_msgs::Odometry>(topic_name_, 100);

    // ros callback queue for processing subscription
    this->callbackQueeuThread = boost::thread(
      boost::bind(&GazeboWorldOdometry::RosQueueThread, this));

    // listen to the update event (broadcast every simulation iteration)
     this->update_connection_ =
     event::Events::ConnectWorldUpdateBegin(
     boost::bind(&GazeboWorldOdometry::UpdateChild, this));
  }

  // Update the controller
  void GazeboWorldOdometry::UpdateChild(){
    if (this->floatingBasePub_.getNumSubscribers() <= 0)
      return;

    boost::mutex::scoped_lock sclock(this->mutex_);

    ignition::math::Pose3d pose;
    ignition::math::Quaterniond orientation;
    ignition::math::Vector3d position;

    pose = this->link->WorldPose();
    position = pose.Pos();
    orientation = pose.Rot();

//    ignition::math::Vector3d linearVel = this->link->WorldLinearVel();
//    ignition::math::Vector3d angularVel = this->link->WorldAngularVel();

    ignition::math::Vector3d linearVel = this->link->WorldLinearVel();
    ignition::math::Vector3d angularVel = this->link->RelativeAngularVel();

    nav_msgs::Odometry odomMsg;

    odomMsg.pose.pose.position.x = position.X();
    odomMsg.pose.pose.position.y = position.Y();
    odomMsg.pose.pose.position.z = position.Z();

    odomMsg.pose.pose.orientation.x = orientation.X();
    odomMsg.pose.pose.orientation.y = orientation.Y();
    odomMsg.pose.pose.orientation.z = orientation.Z();
    odomMsg.pose.pose.orientation.w = orientation.W();

    odomMsg.twist.twist.linear.x = linearVel.X();
    odomMsg.twist.twist.linear.y = linearVel.Y();
    odomMsg.twist.twist.linear.z = linearVel.Z();

    odomMsg.twist.twist.angular.x = angularVel.X();
    odomMsg.twist.twist.angular.y = angularVel.Y();
    odomMsg.twist.twist.angular.z = angularVel.Z();

    odomMsg.header.frame_id = frame_name_;

    floatingBasePub_.publish(odomMsg);
  }


  void GazeboWorldOdometry::RosQueueThread()
  {
  //  static const double timeout = 0.01;
    ros::Rate rate(this->update_rate_);

    while (this->rosNode_->ok())
    {
      this->rosQueue.callAvailable(/*ros::WallDuration(timeout)*/);
      rate.sleep();
    }
  }


  GZ_REGISTER_MODEL_PLUGIN(GazeboWorldOdometry)
}



