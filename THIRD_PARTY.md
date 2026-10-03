# 第三方来源与权利

| 内容 | 来源 | 发布/授权说明 |
| --- | --- | --- |
| `sdk/` | [HelloClyde/bbk9588-bda-sdk](https://github.com/HelloClyde/bbk9588-bda-sdk), `870470ce09a5b33ae9b2d0c1b8e40c0435751a98` | Git 子模块；以 SDK 自身许可证为准。 |
| 构建时获取的四个赛道遍历文件 | [trapexit/3do-decomp-road-rash](https://github.com/trapexit/3do-decomp-road-rash), `97af68f3aabd71b173d8e5b8fed1e03ef106100d` | 上游未提供明确的再分发许可证；不纳入本仓库，`tools/prepare_upstream.py` 在本地获取。对最终二进制的再分发权限也未明确。 |
| `src/road_ui_font.h` | Google Noto Sans SC 字形数据 | 原字体按 [SIL Open Font License 1.1](FONT-OFL.txt) 发布；此文件为字形栅格化衍生数据，保留字体来源与许可。 |
| `assets/road_rash_3do_icon.png` | 3DO 原版《Road Rash》标题画面，裁剪方式见 `assets/README.md` | 原版游戏美术；没有单独的开源授权。MIT 不覆盖此图。 |
| `screenshots/` | 本移植早期模拟器运行画面 | 截图包含原版游戏美术，来源见 README；MIT 不覆盖图像。 |
| `Rash.pak`、音频、赛道与车辆资源 | 原版 3DO 游戏 | 不进入公开仓库或 GitHub Release。由持有资源的用户自行在本地生成。 |

本仓库的开源许可证仅覆盖自编移植代码。尤其需要注意：上游遍历文件与原版游戏图标缺少明确的二进制再分发授权；公开 BDA 为技术预览，使用和再分发应由取得相应权利者自行判断。
