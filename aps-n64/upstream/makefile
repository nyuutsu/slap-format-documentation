FLAGS = -Wall -O3

all : n64caps.exe n64aps.exe

n64caps.exe : n64caps.c
	gcc $(FLAGS) -s -o n64caps.exe n64caps.c
	strip n64caps.exe

n64aps.exe : n64aps.c
	gcc $(FLAGS) -s -o n64aps.exe n64aps.c
	strip n64aps.exe

clean :
	@del n64caps.exe n64aps.exe
