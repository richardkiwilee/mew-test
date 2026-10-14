# mew-test
参考wiki：https://mewgenics.wiki.gg/

mewgenics开发者访谈的描述：
```
A deep dive into the C++ source code of Mewgenics, a game with roughly 250,000 lines of C++ and 250,000 lines of custom data files. The developer walks through the engine architecture including a director/scene/component hierarchy (similar to Unity's entity-component model but without formal systems), a custom game loop with fixed and unlocked updates, Python scripts that auto-generate C++ enums for component reflection, heavy use of C++ macros to reduce boilerplate and enable 'super'/'self' keywords, and frame pacing challenges on Windows including VSync detection and thread scheduler timing jitter. Compile time is around 45-60 seconds for a full rebuild of 122,000 functions, aided by precompiled headers.
```

使用vs2022 c++编写一个尽可能符合mewgenics的战斗框架，如果你需要 可以使用https://github.com/TylerGlaiel/GON  
要求使用python为c++生成反射，代码使用c++生成，使用预编译头来减少编译时间。需要有完整的战斗结算日志帮助我确认是否响应符合预期。
游戏的官方exe目录是D:\Steam\steamapps\common\Mewgenics
必要时，你可以下载逆向工具读取游戏程序的二进制来作为参考。
编译使用cmake, 要求编写build.bat
你可以使用本机的python
你可以使用网络访问参考wiki下的所有子页面
设计架构时要特别注意不同被动对猫猫属性影响的先后顺序，每个行动后其他单位对他的反应，对于部分可能达成无限的技能要做限制。

最终使用D:/godot来调用编译的c++dll构建场景，godot的项目参考是D:\pyFramework\3-dmap-2
需要在当前项目路径下构建godot项目。
尽可能贴合mewgenics的战斗框架
最终的godot结果是一个战斗demo，打开时是战斗准备阶段，随机4只猫的属性、被动，并由我进行装备配置，预设了3个战斗地图，分别有一些不同的地形交互要素。设定几种敌人，完成完整的战斗流程后返回战斗准备阶段。
你需要先制定文档，计划plan，每次完成一个小点都需要即时更新plan

如果你在执行过程中遇到了无法自己解决的问题，需要我进行人工干预，例如你无法下载某个工具，写在user.md中