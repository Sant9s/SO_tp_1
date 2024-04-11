all: slave.elf app.elf view.elf

slave.elf: slave.c pipes.c
	gcc -Wall slave.c pipes.c -o slave.elf -std=c99 -lm -lrt -pthread -g -D_XOPEN_SOURCE=500

app.elf: app.c pipes.c
	gcc -Wall app.c pipes.c -o app.elf -std=c99 -lm -lrt -pthread -g -D_XOPEN_SOURCE=500

view.elf: view.c pipes.c
	gcc -Wall view.c pipes.c -o view.elf -std=c99 -lm -lrt -pthread -g -D_XOPEN_SOURCE=500

clean:
	rm -f slave.elf app.elf view.elf resultado.txt PVS-Studio.log report.tasks strace_out
	rm -rf .config

#Si tienen problemas al hacer make pvs-studio, hagan un make clean y vuelvan a intentar
pvs-studio:
	pvs-studio-analyzer credentials PVS-Studio Free FREE-FREE-FREE-FREE
	pvs-studio-analyzer trace -- make -j8
	pvs-studio-analyzer analyze -j2 -l /root/.config/PVS-Studio/PVS-Studio.lic -o PVS-Studio.log
	plog-converter -a GA:1,2 -t tasklist -o report.tasks PVS-Studio.log

.PHONY: all clean
