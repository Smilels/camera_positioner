#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <apriltags_ros/AprilTagDetectionArray.h>

#include <sensor_msgs/JointState.h>


namespace {
void interpolateTransforms(const tf::Transform& t1, const tf::Transform& t2, double fraction, tf::Transform& t_out){
	t_out.setOrigin( t1.getOrigin()*(1-fraction) + t2.getOrigin()*fraction );
	t_out.setRotation( t1.getRotation().slerp(t2.getRotation(), fraction) );
}
}

class CameraPositioner {
private:
   ros::Subscriber sub;

   // TF communication channels
   tf::TransformListener listener;
   tf::TransformBroadcaster br;

   // constant transforms
   tf::StampedTransform optical_transform;
   tf::StampedTransform world_tag_transform;
   tf::StampedTransform table_tag_transform;

   tf::Transform world_tag17_transform;
   tf::Transform tag17_transform;
//   tf::Transform tag17_table_transform;

   // latest measured position of the camera
   tf::Transform world_camera_transform;
   ros::Time latest_detection_time;

   // for successful initialization the apriltag has to be detected _once_
   bool initialized;
   int get_tag17_transform;
   float tag17_size;

   double filter_weight;

   // name of the frames of the camera
   std::string camera_link;
   std::string camera_rgb_optical_frame;

public:
   CameraPositioner() : initialized(false),get_tag17_transform(0), filter_weight(0.15)
   {
      ros::NodeHandle node;
      ros::NodeHandle private_node("~");
      private_node.param<std::string>("camera_rgb_optical_frame", camera_rgb_optical_frame, "/camera_rgb_optical_frame");
      private_node.param<std::string>("camera_link", camera_link, "/camera_link");
      getConstantTransforms();
      sub = node.subscribe("tag_detections", 1, &CameraPositioner::callback, this);
   }

   void getConstantTransforms(){
      while(true){
         try {
            listener.waitForTransform("/world", "/april_tag_ur5", ros::Time(0), ros::Duration(5.0) );
            listener.lookupTransform("/world", "/april_tag_ur5", ros::Time(0), world_tag_transform);
            break;
         }
         catch(...){}
         ROS_WARN_THROTTLE(10, "Waiting for world->april_tag_ur5 transform");
      }

      while(true){
         try {
            listener.waitForTransform(camera_rgb_optical_frame, camera_link, ros::Time(0), ros::Duration(5.0) );
            listener.lookupTransform(camera_rgb_optical_frame, camera_link,  ros::Time(0), optical_transform);
            break;
         }
         catch(...){}
         ROS_WARN_THROTTLE(10, "Waiting for camera_rgb_optical_frame->camera_link transform");
      }

      while(true){
         try {
            listener.waitForTransform("/table_tag", "/table", ros::Time(0), ros::Duration(5.0) );
            listener.lookupTransform("/table_tag", "/table",  ros::Time(0), table_tag_transform);
            break;
         }
         catch(...){}
         ROS_WARN_THROTTLE(10, "Waiting for table_tag->table transform");
      }
   }

   void callback(const apriltags_ros::AprilTagDetectionArray& msg){
      //check whether tag0 is detected, update world_camera_transform
      bool get_tag0=false;
      int get_tag17=false;
      for (int i=0; i< msg.detections.size(); i++) {
        if(msg.detections[i].id == 0){
          tf::Transform tag_transform;
          tf::poseMsgToTF(msg.detections[i].pose.pose, tag_transform);
          if(!initialized){
             ROS_INFO("camera positioner is running");
             initialized = true;
             world_camera_transform= world_tag_transform * tag_transform.inverse() * optical_transform;
          }
          else {
             tf::Transform world_camera_transform_new;
             interpolateTransforms(world_camera_transform, world_tag_transform * tag_transform.inverse() * optical_transform, filter_weight, world_camera_transform_new);
             world_camera_transform= world_camera_transform_new;
          }
          latest_detection_time = msg.detections[0].pose.header.stamp;
          get_tag0=true;
        }
        if(msg.detections[i].id == 42){
          get_tag17=true;
          tag17_size=msg.detections[i].size;
	  tf::poseMsgToTF(msg.detections[i].pose.pose, tag17_transform);
        }
     }

      // get tag17 position based on tag0 at the beginning
      // if get_tag0, update world_camera_transform and world_tag17_transform based on tag0
      // if only get_tag17 , update world_camera_transform based on tag17
      if(get_tag17){
           latest_detection_time = msg.detections[0].pose.header.stamp;
           if(get_tag17_transform==0){
              if(get_tag0){
                  world_tag17_transform=world_camera_transform * optical_transform.inverse() *tag17_transform;
                  get_tag17_transform++;
              }
	           else
                  ROS_ERROR("Please adjust camera to see two Apriltags");
          }
          else
              if(get_tag0){
		tf::Transform world_tag17_transform_new;
                interpolateTransforms(world_tag17_transform, world_camera_transform * optical_transform.inverse() *tag17_transform, filter_weight, world_tag17_transform_new);
                world_tag17_transform = world_tag17_transform_new;
                get_tag17_transform++;
              }
              else {
                tf::Transform world_camera_transform_new;
                interpolateTransforms(world_camera_transform, world_tag17_transform * tag17_transform.inverse() * optical_transform, filter_weight, world_camera_transform_new);
                world_camera_transform = world_camera_transform_new;
              }
      }

      if(ros::Time::now() - latest_detection_time > ros::Duration(25.0)){
         ROS_WARN_THROTTLE(5, "Didn't detect apriltag for camera position update in 20 seconds. The camera might have moved in the meanwhile.");
      }

      // if we measured the camera's position successfully, publish it
      if(initialized){
         br.sendTransform(tf::StampedTransform(world_camera_transform, latest_detection_time, "/world", camera_link));
      }
      // publish tag17 and real_table_top
      if (get_tag17_transform!=0){
          br.sendTransform(tf::StampedTransform(world_tag17_transform*table_tag_transform, latest_detection_time, "/world", "/table"));
      }
    }
};

int main(int argc, char** argv){
  ros::init(argc, argv, "camera_position_node1");
  CameraPositioner cam_pos;
  ros::spin();
  return 0;
};
