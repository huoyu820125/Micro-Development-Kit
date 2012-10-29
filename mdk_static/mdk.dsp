# Microsoft Developer Studio Project File - Name="mdk" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mdk - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mdk.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mdk.mak" CFG="mdk - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mdk - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mdk - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mdk - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x804 /d "NDEBUG"
# ADD RSC /l 0x804 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "mdk - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\mdk_d.lib"

!ENDIF 

# Begin Target

# Name "mdk - Win32 Release"
# Name "mdk - Win32 Debug"
# Begin Group "mdk"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\include\mdk\atom.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\ConfigFile.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\ConfigFile.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Executor.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Executor.h
# End Source File
# Begin Source File

SOURCE=..\include\mdk\FixLengthInt.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\IOBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\IOBuffer.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\IOBufferBlock.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\IOBufferBlock.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Lock.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Lock.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Logger.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Logger.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\MemoryPool.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\MemoryPool.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Queue.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Queue.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Signal.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Signal.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Socket.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Socket.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Task.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Task.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\Thread.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\Thread.h
# End Source File
# Begin Source File

SOURCE=..\source\mdk\ThreadPool.cpp
# End Source File
# Begin Source File

SOURCE=..\include\mdk\ThreadPool.h
# End Source File
# End Group
# Begin Group "frame"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "netserver"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\source\frame\netserver\EpollFrame.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\EpollFrame.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\EpollMonitor.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\EpollMonitor.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\IOCPFrame.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\IOCPFrame.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\IOCPMonitor.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\IOCPMonitor.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\NetConnect.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\NetConnect.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\NetEngine.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\NetEngine.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\NetEventMonitor.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\NetEventMonitor.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\NetHost.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\NetHost.h
# End Source File
# Begin Source File

SOURCE=..\source\frame\netserver\NetServer.cpp
# End Source File
# Begin Source File

SOURCE=..\include\frame\netserver\NetServer.h
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=.\makefile
# End Source File
# End Target
# End Project
