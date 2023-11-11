<div align="center">

# Smallchat-Win32
[English](./README.md) / 简体中文
</div>

本仓库中的代码是基于 Salvatore Sanfilippo 创建的 [antirez/Smallchat](https://github.com/antirez/smallchat) 的 Win32 移植版本。


上周五，我在这个项目上做了一些工作，让整个程序在 Win32 平台上运行起来了。服务器端已经可以正常工作了，但没有进行太多测试。用`nc`做客户端做了测试，看起来没什么问题。客户端仍有一些与控制台非阻塞输入相关的问题。但我认为它可以在VSCode 的集成终端上正常运行的，对于`Terminal`和控制台的支持还有待确认。

我将关注原 `Smallchat` 的更新，并尝试迁移到 Win32 平台。如果您对此感兴趣，您的加入将非常有帮助。

我用 VSCode + CMake 和 VS2022 移植了代码。不过，Win32 下的其他编译器也应该可以工作。如果发现任何问题，请提交一个issue来告知我。

`Smallchat`原始的[README](README_Smallchat.md)文件可以点击这个链接.






