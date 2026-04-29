# face_detection

## 项目简介

ROS2 人脸检测节点，基于 `vision_service.h` 调用 `model_zoo/vision`（`libvision`）实现实时人脸检测功能。

## 功能特性

- 支持实时人脸检测
- 输出标准 Detection2DArray 格式
- 可视化调试图像
- 支持直连摄像头或订阅图像话题
- 不支持：人脸识别、人脸特征提取

## 快速开始

### 环境准备

- ROS2 Humble 或更高版本
- 已编译的 `components/model_zoo/vision` 组件
- 对应的模型文件

### 构建编译

```bash
colcon build --packages-select face_detection
source install/setup.bash
```

### 运行示例

```bash
ros2 launch face_detection face_detection.launch.py
```

## 详细使用


### 依赖

- `components/model_zoo/vision`：提供 `libvision.so` 与 `vision_service.h`
- 人脸检测模型及对应 yaml 配置

### 话题

| 类型 | 话题（默认） | 说明 |
|------|--------------|------|
| 订阅 | `/camera/image_raw` | 输入图像 |
| 发布 | `/perception/faces` | `vision_msgs/Detection2DArray`（需 `vision_msgs`） |
| 发布 | `/face_detection/boxes` | 检测框中间结果 |
| 发布 | `/face_detection/debug_image` | 可视化图 |

### 配置

主要配置文件位于包内 `config/` 目录，通常包含：

- 模型路径
- 置信度阈值
- 输入输出话题名
- 是否直连摄像头等参数


## 常见问题

### 无相机时用图片测试

```bash
ros2 run face_detection publish_image /path/to/face.jpg
```

## 版本与发布

当前版本：1.0.0

变更记录：
- 初始版本发布

## 贡献方式

欢迎提交 Issue 和 Pull Request。

贡献者与维护者名单见：`CONTRIBUTORS.md`（如有）

## License

本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
