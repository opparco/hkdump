
CPPFLAGS = /nologo /EHsc /MT /I ..\vendor\hk2010_2_0_r1\Source
CONFLAGS = /nologo /LIBPATH:..\vendor\hk2010_2_0_r1\Lib\win32_net_9-0\release_multithreaded

all: hkconv.exe hkdump.exe hkdump-bin.exe

clean:
	rm *.obj

hkconv.exe: conv.obj
	link.exe $(CONFLAGS) /OUT:hkconv.exe conv.obj legacy_stdio_definitions.lib

hkdump.exe: dump.obj
	link.exe $(CONFLAGS) /OUT:hkdump.exe dump.obj legacy_stdio_definitions.lib

hkdump-bin.exe: dump-bin.obj
	link.exe $(CONFLAGS) /OUT:hkdump-bin.exe dump-bin.obj legacy_stdio_definitions.lib
