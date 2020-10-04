CFLAGS = /nologo /W3 /EHsc /D_CRT_SECURE_NO_DEPRECATE
CC = cl
LINK = link

build: libscheduler.dll

libscheduler.dll: so_scheduler.obj queue.obj
	$(LINK) /nologo /dll /out:$@ /implib:libscheduler.lib $**

so_scheduler.obj: so_scheduler.c
	$(CC) $(CFLAGS) /Fo$@ /c $**

queue.obj: queue.c
	$(CC) $(CFLAGS) /Fo$@ /c $**

clean:
	del /Q so_scheduler.obj queue.obj 2>NUL
	del /Q libscheduler.dll libscheduler.lib libscheduler.exp 2>NUL
