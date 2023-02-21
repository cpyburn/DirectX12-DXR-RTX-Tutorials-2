DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

DXR Tutorial 1
Creating a Window

Introduction
Hello everyone and welcome to the DXR tutorial series. Over the course of the next 14 tutorials, we will cover all the basics of the DXR API. 
The tutorials will only cover the DXR API. It is assumed that the reader has good knowledge of DirectX12 and Windows programming.
Let’s begin…

The Tutorials Framework
The first thing to pay attention to, is that the solution contains a project called Framework. As the name suggests, it’s a simple abstraction layer for the windowing system, and also provides a few useful utilities shared by the tutorials.
It also provides very simple keyboard functionality – pressing the Escape key will close the application.
01-CreateWindow.h
This is where most of the code for the tutorial is.
The only class in this file is Tutorial01 which inherits from Tutorial. This base class provides the windows and messaging abstraction. As you can see, we are overriding 3 base-class functions. These functions are callbacks which will be called during different times in the application’s lifetime:

onLoad()
Called once at the beginning. The window is already created at this point and we can use the window handle safely. This is where we will initialize the different API objects.

onFrameRender()
The main render function. This is where will create and submit graphics commands.

onShutdown()
Called right before the application terminates. This is where we want to place all the cleanup code. You’ll notice that we are using smart pointers and smart COM pointers, so this function is not really used.

Well, that’s it for the header. Time to move to the CPP file.

01-CreateWindow.cpp
Nothing much here. We included “01-CreateWindow.h” and added empty definitions of the required callback.
If you’ll scroll to the bottom of the file, you’ll see the WinMain() function. It contains a single line of code:



As you might have guessed, the first parameter is our Tutorial01 object and the second parameter is the window’s title.
Running this application doesn’t yield much. Just an empty window.

Conclusion
We didn’t do much here. Just creating a window. In the next tutorial we will start using DXR.
