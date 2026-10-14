# 需人工干预事项

（空）—— 截至 2026-10-14 无阻塞问题：
- 本机工具链齐全（VS2022 自带 CMake/Ninja、Python 3.14、Godot 4.6.2、godot-cpp 10.0.0-rc1）；
- 未使用逆向工具（wiki 调研已覆盖所需规则，Mewgenics.exe 未解析）；
- 所有构建、测试、Demo 流程已自动验证通过，无需人工介入。

若后续需要：
1. 真机游玩 Demo：双击 Godot 运行 godot/project.godot（或 godot --path godot），先跑 build.bat 生成 DLL。
2. 参考原游戏二进制（Mewgenics.exe + resources.gpak）：可用 Ghidra/IDA 加载，但本框架不依赖其结果。
