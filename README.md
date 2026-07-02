# SNDWAY SW-3000A&B 上位机软件
基于 Qt 开发的串口测量设备上位机，支持测距、测高、角度采集、实时曲线绘制、数据保存等功能。

## 功能列表
1. 串口通信（8N1，默认波特率 115200）
2. 设备模式切换、亮度/单位/马达控制
3. 单次/连续测量，电池状态监测
4. 数据表格展示 + QCustomPlot 实时曲线绘图
5. 数据导出为 TXT / CSV 日志文件
6. 配置本地保存与加载

## 编译环境
- 开发框架：Qt 社区版（Qt5）
- 第三方组件：QCustomPlot 2.1.0

## 编译方式
1. 使用 Qt Creator 打开 `SNDWAY_SW3000A_B.pro`
2. 选择 Release 模式编译
3. 使用 `windeployqt` 部署运行库

## 开源协议
本项目基于 **GNU General Public License v3.0** 开源，详情见 LICENSE 文件。