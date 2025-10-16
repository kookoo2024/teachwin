# TeachingWindow 教学工具箱

一个功能强大的Windows教学辅助工具，专为教师教室管理而设计。

## ✨ 主要功能

### 📚 核心功能
- **电脑浏览器** - 快速打开配置的教学网页资源
- **随机点名系统** - 美化动画效果的随机学生点名
- **TV/备课/班级链接** - 一键打开教学相关网页

### 🎨 界面特色
- 浮动窗口设计，占用屏幕空间最小化
- 美化点名窗口，华丽动画效果
- 可拖动的主窗口和设置窗口
- 无DOS控制台窗口的纯GUI程序

### ⚙️ 设置功能
- **URL配置** - 为各按钮配置自定义网页链接
- **动画设置** - 定制点名动画持续时间 (500ms-1500ms)
- **学生名单管理** - 支持空格、逗号分隔学生姓名
- **配置文件持久化** - 设置自动保存到本地

## 🚀 使用方法

1. **启动工具**：双击 `TeachingWindow.exe` 运行
2. **点名功能**：点击"点名"按钮体验美化随机点名
3. **网页访问**：点击"电脑"、"TV"等按钮打开配置的网页
4. **个性化设置**：点击"补充"按钮进入设置窗口

## 🔧 构建要求

- **编译器**: MinGW-w64 (g++)
- **Windows SDK**: Windows API
- **编译命令**:
  ```bash
  g++ TeachingWindow.cpp -o TeachingWindow.exe -std=c++17 -luser32 -lgdi32 -lshell32 -mwindows
  ```

## 📁 文件结构

```
TeachingWindow/
├── TeachingWindow.cpp      # 主程序源代码
├── TeachingWindow.exe      # 可执行文件
├── README.md               # 项目说明文档
└── config.json             # 用户配置文件 (自动生成)
```

## 💾 配置说明

配置文件保存在 `%APPDATA%\TeachingWindow\config.json`

示例配置：
```json
{
  "manual_ip": "192.168.6.155",
  "inner_url": "https://www.example.com",
  "students": "|张三|李四|王五|赵六|",
  "button3_url": "https://www.baidu.com",
  "button4_url": "https://www.google.com",
  "button5_url": "https://www.github.com",
  "roll_duration": 800
}
```

## 🎯 特色功能

- **动画点名**: 华丽的滚动动画，营造悬念效果
- **一键访问**: 预设网页链接，教学更高效
- **个性化设置**: 从动画间隔到网页链接，全可定制
- **学生管理**: 灵活的学生名单录入方式
- **响应式设计**: 适配不同分辨率的屏幕

## 🤝 贡献

欢迎提交ISSUE和Pull Request！

## 📋 TODO

- [ ] 添加学生名单导入CSV功能
- [ ] 支持更多主题样式
- [ ] 添加音频提示功能
- [ ] 统计点名次数记录

## 📄 许可证

本项目使用MIT许可证。
