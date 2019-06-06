
CPPFLAGS = /nologo /EHsc /MD /I ..\vendor\hk2014_1_0_r1\Source
CONFLAGS = /nologo /LIBPATH:..\vendor\hk2014_1_0_r1\Lib\win32_vs2012_win7\release

all: hkconv.exe hkdump.exe hkdump-bin.exe

clean:
	rm *.obj

hkconv.exe: conv.obj
	link.exe $(CONFLAGS) /OUT:hkconv.exe conv.obj 

hkdump.exe: dump.obj
	link.exe $(CONFLAGS) /OUT:hkdump.exe dump.obj 

hkdump-bin.exe: dump-bin.obj
	link.exe $(CONFLAGS) /OUT:hkdump-bin.exe dump-bin.obj 
