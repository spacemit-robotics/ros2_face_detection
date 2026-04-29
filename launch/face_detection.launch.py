"""Launch face_detection node with config."""
from launch import LaunchDescription
from launch_ros.substitutions import FindPackageShare
from launch_ros.actions import Node
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():
    params_file = PathJoinSubstitution(
        [FindPackageShare("face_detection"), "config", "face_detection.yaml"]
    )
    return LaunchDescription(
        [
            Node(
                package="face_detection",
                executable="face_detection_node",
                name="face_detection_node",
                output="screen",
                parameters=[params_file],
            )
        ]
    )
