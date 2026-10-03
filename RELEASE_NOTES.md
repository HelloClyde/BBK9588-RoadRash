# 暴力摩托 9588 v0.1.2 预览版

本版修正本地 `Rash.pak` 生成顺序：`rashOpt.rsrc` 位于包首。旧包虽然包含背景图，但模拟器读取包尾的车辆与路面纹理资源失败；重新运行 `python tools/build_resource_pack.py --source <原版Rash目录>` 后，主菜单背景、摩托车、仪表和路面纹理已在模拟器中验证。README 同时换成四张当前模拟器截图，并重排使用与开发说明。

`RoadRash.bda` 的程序内容与 v0.1.1 相同，SHA-256 为 `bd522d6dcb351e59eeafe7466aaf0a95b81420d969fb0229d07d65ac9cb38e42`。安装 BDA 后，需自行生成 `Rash.pak`，放在 `B:\应用\数据\游戏\Rash\Rash.pak`，或没有 B: 盘时放在 A: 对应路径。**本 Release 不包含原版游戏资源。**

CI 对 BDA 执行构建和格式校验；该确切二进制尚未完成 9588 真机验收。
