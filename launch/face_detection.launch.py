"""Launch face_detection node with config."""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch_ros.substitutions import FindPackageShare
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution


def generate_launch_description():
    params_file = PathJoinSubstitution(
        [FindPackageShare("face_detection"), "config", "face_detection.yaml"]
    )
    return LaunchDescription(
        [
            DeclareLaunchArgument("use_camera", default_value="true"),
            DeclareLaunchArgument("camera_id", default_value="1"),
            DeclareLaunchArgument("score_threshold", default_value="0.3"),
            Node(
                package="face_detection",
                executable="face_detection_node",
                name="face_detection_node",
                output="screen",
                parameters=[
                    params_file,
                    {
                        "use_camera": LaunchConfiguration("use_camera"),
                        "camera_id": LaunchConfiguration("camera_id"),
                        "score_threshold": LaunchConfiguration("score_threshold"),
                    },
                ],
            )
        ]
    )
