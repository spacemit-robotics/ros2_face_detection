/**
 * @file face_detection_node.cpp
 * @brief Face detection node: subscribes to image, runs face model,
 *        publishes boxes and optional Detection2DArray, debug image.
 *
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"
#include "face_detection/detection_utils.h"
#include "face_detection/image_utils.h"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"
#include "vision_service.h"
#ifdef HAVE_VISION_MSGS
#include "vision_msgs/msg/detection2_d_array.hpp"
#endif

class FaceDetectionNode : public rclcpp::Node {
    public:
    FaceDetectionNode() : Node("face_detection_node") {
        std::string config_path = declare_parameter<std::string>("config_path", "");
        const bool lazy_load = declare_parameter<bool>("lazy_load", true);
        score_threshold_ = declare_parameter<double>("score_threshold", 0.3);
        if (!std::isfinite(score_threshold_) || score_threshold_ < 0.0 ||
            score_threshold_ > 1.0) {
            throw std::runtime_error("score_threshold out of range, must be in [0, 1]");
        }
        face_label_name_ = declare_parameter<std::string>("face_label_name", "face");
        image_topic_ = declare_parameter<std::string>("image_topic", "/camera/image_raw");
        debug_image_topic_ =
            declare_parameter<std::string>("debug_image_topic", "/face_detection/debug_image");
        boxes_topic_ = declare_parameter<std::string>("boxes_topic", "/face_detection/boxes");
        use_camera_ = declare_parameter<bool>("use_camera", true);
        camera_id_ = declare_parameter<int>("camera_id", 0);
        camera_fps_ = declare_parameter<double>("camera_fps", 30.0);
#ifdef HAVE_VISION_MSGS
        faces_topic_ = declare_parameter<std::string>("faces_topic", "/perception/faces");
#endif

        if (config_path.empty()) {
            config_path = GetDefaultConfigPath();
        }
        if (config_path.empty()) {
            throw std::runtime_error(
                "config_path is empty. Set the 'config_path' parameter to the vision model config "
                "yaml (e.g. share/face_detection/config/yolov5-face.yaml) or install the package "
                "and use the default.");
        }

        service_ = VisionService::Create(config_path, "", lazy_load);
        if (!service_) {
            throw std::runtime_error("VisionService::Create failed: " +
                                        VisionService::LastCreateError());
        }

        boxes_pub_ = create_publisher<std_msgs::msg::Float32MultiArray>(boxes_topic_, 10);
#ifdef HAVE_VISION_MSGS
        faces_pub_ = create_publisher<vision_msgs::msg::Detection2DArray>(faces_topic_, 10);
#endif
        debug_pub_ = create_publisher<sensor_msgs::msg::Image>(debug_image_topic_,
                                                                rclcpp::SensorDataQoS());

        if (use_camera_) {
            image_pub_ = create_publisher<sensor_msgs::msg::Image>(image_topic_,
                                                                    rclcpp::SensorDataQoS());
            cap_.open(camera_id_);
            if (!cap_.isOpened()) {
                throw std::runtime_error("face_detection: cannot open camera id=" +
                                            std::to_string(camera_id_));
            }
            const int period_ms =
                (camera_fps_ > 1.0) ? static_cast<int>(1000.0 / camera_fps_) : 33;
            camera_timer_ = create_wall_timer(
                std::chrono::milliseconds(period_ms),
                std::bind(&FaceDetectionNode::OnCameraTimer, this));
            RCLCPP_INFO(get_logger(),
                        "face_detection_node: use_camera=true, publishing to %s, camera_id=%d",
                        image_topic_.c_str(), camera_id_);
        } else {
            image_sub_ = create_subscription<sensor_msgs::msg::Image>(
                image_topic_, rclcpp::SensorDataQoS(),
                std::bind(&FaceDetectionNode::OnImage, this, std::placeholders::_1));
            RCLCPP_INFO(get_logger(),
                        "face_detection_node: use_camera=false, subscribing to %s",
                        image_topic_.c_str());
        }

        RCLCPP_INFO(get_logger(),
                    "face_detection_node started, config=%s, image_topic=%s, score_threshold=%.2f",
                    config_path.c_str(), image_topic_.c_str(), score_threshold_);
    }

    ~FaceDetectionNode() override = default;

    private:
    static std::string GetDefaultConfigPath() {
        try {
            return ament_index_cpp::get_package_share_directory("face_detection") +
                    "/config/yolov5-face.yaml";
        } catch (...) {
            return "";
        }
    }

    void OnCameraTimer() {
        std_msgs::msg::Header header;
        header.stamp = now();
        header.frame_id = "camera";
        sensor_msgs::msg::Image img_msg;
        if (!face_detection::CaptureCameraFrame(cap_, img_msg, header)) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 3000, "camera read empty frame");
            return;
        }
        image_pub_->publish(img_msg);
        cv::Mat bgr = face_detection::ImageMsgToBgr(img_msg);
        if (bgr.empty()) return;
        ProcessFrame(img_msg.header, bgr);
    }

    void OnImage(const sensor_msgs::msg::Image::SharedPtr msg) {
        cv::Mat bgr = face_detection::ImageMsgToBgr(*msg);
        if (bgr.empty()) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 3000,
                                    "invalid or unsupported image (encoding=%s)",
                                    msg->encoding.c_str());
            return;
        }
        ProcessFrame(msg->header, bgr);
    }

    void ProcessFrame(const std_msgs::msg::Header& header, const cv::Mat& bgr) {
        VisionServiceResponse response;
        if (service_->Infer(bgr, &response) != VISION_SERVICE_OK || !response.ok) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "infer failed: %s",
                                    service_->LastError().c_str());
            return;
        }

        std::vector<face_detection::DetectionBox> boxes;
        boxes.reserve(response.results.size());
        for (const auto& r : response.results) {
            const auto* det = std::get_if<vision::Detection>(&r);
            if (det == nullptr || det->score < static_cast<float>(score_threshold_)) continue;
            face_detection::DetectionBox b;
            b.x1 = det->bbox.x1;
            b.y1 = det->bbox.y1;
            b.x2 = det->bbox.x2;
            b.y2 = det->bbox.y2;
            b.score = det->score;
            b.label = det->label;
            b.track_id = -1;
            b.class_name = face_label_name_;
            boxes.push_back(b);
        }

        boxes_pub_->publish(face_detection::EncodeBoxes(boxes, "num_faces"));
#ifdef HAVE_VISION_MSGS
        faces_pub_->publish(face_detection::EncodeDetection2DArray(boxes, header));
#endif
        PublishDebugImage(header, bgr, response);
    }

    void PublishDebugImage(const std_msgs::msg::Header& header, const cv::Mat& bgr,
                            const VisionServiceResponse& response) {
        cv::Mat out_image;
        if (service_->Draw(bgr, response, &out_image) != VISION_SERVICE_OK) {
            return;
        }
        if (out_image.empty() || out_image.rows != bgr.rows || out_image.cols != bgr.cols) {
            return;
        }
        debug_pub_->publish(face_detection::BgrToImageMsg(out_image, header, "bgr8"));
    }

    std::unique_ptr<VisionService> service_;
    double score_threshold_{0.3};
    std::string face_label_name_;
    std::string image_topic_;
    std::string debug_image_topic_;
    std::string boxes_topic_;
    bool use_camera_{true};
    int camera_id_{0};
    double camera_fps_{30.0};
    cv::VideoCapture cap_;
    rclcpp::TimerBase::SharedPtr camera_timer_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
#ifdef HAVE_VISION_MSGS
    std::string faces_topic_;
    rclcpp::Publisher<vision_msgs::msg::Detection2DArray>::SharedPtr faces_pub_;
#endif
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr boxes_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr debug_pub_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    try {
        auto node = std::make_shared<FaceDetectionNode>();
        rclcpp::spin(node);
    } catch (const std::exception& e) {
        RCLCPP_ERROR(rclcpp::get_logger("face_detection_node"), "Exception: %s", e.what());
        rclcpp::shutdown();
        return 1;
    }
    rclcpp::shutdown();
    return 0;
}
