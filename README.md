<div align="center">

# Smallchat-Win32
English / [简体中文](./README_CN.md)

</div>
This is a Win32 porting of <a href="https://github.com/antirez/smallchat">antirez/Smallchat</a> created by Salvatore Sanfilippo.


Last Friday, I did some work on this project to get the whole program running on the Win32 platform. Now the server can work properly but without much testing. It seems work well with `nc` client. There are still some non-blocking console input related issuses for the client. But I think it can work correctly in the integrated terminal of VSCode.

I will follow the updates of the original `Smallchat` and try to migrate to Win32 platform. If you have any interest in this, your inclusion is very helpful.

I ported the code with VSCode + CMake and VS2022. But other compiler under Win32 should work too. If you catch any problem, please open an issue.

The original [README](README_Smallchat.md) is here.






