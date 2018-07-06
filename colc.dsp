# Microsoft Developer Studio Project File - Name="colc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=colc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "colc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "colc.mak" CFG="colc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "colc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "colc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "colc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "colc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "colc - Win32 Release"
# Name "colc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\act.build.c
# End Source File
# Begin Source File

SOURCE=.\src\act.comm.c
# End Source File
# Begin Source File

SOURCE=.\src\act.informative.c
# End Source File
# Begin Source File

SOURCE=.\src\act.item.c
# End Source File
# Begin Source File

SOURCE=.\src\act.movement.c
# End Source File
# Begin Source File

SOURCE=.\src\act.offensive.c
# End Source File
# Begin Source File

SOURCE=.\src\act.other.c
# End Source File
# Begin Source File

SOURCE=.\src\act.social.c
# End Source File
# Begin Source File

SOURCE=.\src\act.wizard.c
# End Source File
# Begin Source File

SOURCE=.\src\alias.c
# End Source File
# Begin Source File

SOURCE=.\src\ban.c
# End Source File
# Begin Source File

SOURCE=.\src\boards.c
# End Source File
# Begin Source File

SOURCE=.\src\castle.c
# End Source File
# Begin Source File

SOURCE=.\src\clan.c
# End Source File
# Begin Source File

SOURCE=.\src\class.c
# End Source File
# Begin Source File

SOURCE=.\src\comm.c
# End Source File
# Begin Source File

SOURCE=.\src\config.c
# End Source File
# Begin Source File

SOURCE=.\src\constants.c
# End Source File
# Begin Source File

SOURCE=.\src\db.c
# End Source File
# Begin Source File

SOURCE=.\src\economy.c
# End Source File
# Begin Source File

SOURCE=.\src\fight.c
# End Source File
# Begin Source File

SOURCE=.\src\genmob.c
# End Source File
# Begin Source File

SOURCE=.\src\genobj.c
# End Source File
# Begin Source File

SOURCE=.\src\genolc.c
# End Source File
# Begin Source File

SOURCE=.\src\genshp.c
# End Source File
# Begin Source File

SOURCE=.\src\genwld.c
# End Source File
# Begin Source File

SOURCE=.\src\genzon.c
# End Source File
# Begin Source File

SOURCE=.\src\gladiator.c
# End Source File
# Begin Source File

SOURCE=.\src\graph.c
# End Source File
# Begin Source File

SOURCE=.\src\handler.c
# End Source File
# Begin Source File

SOURCE=.\src\house.c
# End Source File
# Begin Source File

SOURCE=".\src\improved-edit.c"
# End Source File
# Begin Source File

SOURCE=.\src\interpreter.c
# End Source File
# Begin Source File

SOURCE=.\src\kingdom.c
# End Source File
# Begin Source File

SOURCE=.\src\limits.c
# End Source File
# Begin Source File

SOURCE=.\src\magic.c
# End Source File
# Begin Source File

SOURCE=.\src\mail.c
# End Source File
# Begin Source File

SOURCE=.\src\medit.c
# End Source File
# Begin Source File

SOURCE=.\src\mobact.c
# End Source File
# Begin Source File

SOURCE=.\src\modify.c
# End Source File
# Begin Source File

SOURCE=.\src\oasis.c
# End Source File
# Begin Source File

SOURCE=.\src\objsave.c
# End Source File
# Begin Source File

SOURCE=.\src\oedit.c
# End Source File
# Begin Source File

SOURCE=.\src\olc.c
# End Source File
# Begin Source File

SOURCE=.\src\race.c
# End Source File
# Begin Source File

SOURCE=.\src\random.c
# End Source File
# Begin Source File

SOURCE=.\src\redit.c
# End Source File
# Begin Source File

SOURCE=.\src\sedit.c
# End Source File
# Begin Source File

SOURCE=.\src\shop.c
# End Source File
# Begin Source File

SOURCE=.\src\spec_assign.c
# End Source File
# Begin Source File

SOURCE=.\src\spec_procs.c
# End Source File
# Begin Source File

SOURCE=.\src\spell_parser.c
# End Source File
# Begin Source File

SOURCE=.\src\spells.c
# End Source File
# Begin Source File

SOURCE=.\src\tedit.c
# End Source File
# Begin Source File

SOURCE=.\src\utils.c
# End Source File
# Begin Source File

SOURCE=.\src\weather.c
# End Source File
# Begin Source File

SOURCE=.\src\zedit.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\src\boards.h
# End Source File
# Begin Source File

SOURCE=.\src\clan.h
# End Source File
# Begin Source File

SOURCE=.\src\comm.h
# End Source File
# Begin Source File

SOURCE=.\src\conf.h
# End Source File
# Begin Source File

SOURCE=.\src\constants.h
# End Source File
# Begin Source File

SOURCE=.\src\db.h
# End Source File
# Begin Source File

SOURCE=.\src\economy.h
# End Source File
# Begin Source File

SOURCE=.\src\genmob.h
# End Source File
# Begin Source File

SOURCE=.\src\genobj.h
# End Source File
# Begin Source File

SOURCE=.\src\genolc.h
# End Source File
# Begin Source File

SOURCE=.\src\genshp.h
# End Source File
# Begin Source File

SOURCE=.\src\genwld.h
# End Source File
# Begin Source File

SOURCE=.\src\genzon.h
# End Source File
# Begin Source File

SOURCE=.\src\handler.h
# End Source File
# Begin Source File

SOURCE=.\src\house.h
# End Source File
# Begin Source File

SOURCE=".\src\improved-edit.h"
# End Source File
# Begin Source File

SOURCE=.\src\interpreter.h
# End Source File
# Begin Source File

SOURCE=.\src\mail.h
# End Source File
# Begin Source File

SOURCE=.\src\oasis.h
# End Source File
# Begin Source File

SOURCE=.\src\olc.h
# End Source File
# Begin Source File

SOURCE=.\src\royal.h
# End Source File
# Begin Source File

SOURCE=.\src\screen.h
# End Source File
# Begin Source File

SOURCE=.\src\shop.h
# End Source File
# Begin Source File

SOURCE=.\src\spells.h
# End Source File
# Begin Source File

SOURCE=.\src\structs.h
# End Source File
# Begin Source File

SOURCE=.\src\sysdep.h
# End Source File
# Begin Source File

SOURCE=.\src\tedit.h
# End Source File
# Begin Source File

SOURCE=.\src\telnet.h
# End Source File
# Begin Source File

SOURCE=.\src\utils.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
