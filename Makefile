include ./Makefile.inc

all: server admin

server:
	cd $(SERVER_DIR); make all
	mkdir -p $(TARGET_DIR)
	cp $(SERVER_DIR)/$(SERVER_NAME) $(TARGET_DIR)/$(SERVER_NAME)
	rm -f $(SERVER_DIR)/$(SERVER_NAME) 

admin:
	cd $(ADMIN_DIR); make all
	mkdir -p $(TARGET_DIR)
	cp $(ADMIN_DIR)/$(ADMIN_NAME) $(TARGET_DIR)/$(ADMIN_NAME)
	rm -f $(ADMIN_DIR)/$(ADMIN_NAME)

clean:
	rm -rf $(TARGET_DIR)
	rm -rf $(LOG_DIR)
	cd $(SERVER_DIR); make clean
	cd $(ADMIN_DIR); make clean

install_pvs_studio:
	@wget -q -O - https://files.pvs-studio.com/etc/pubkey.txt | apt-key add -
	@wget -O /etc/apt/sources.list.d/viva64.list https://files.pvs-studio.com/etc/viva64.list
	@apt-get install apt-transport-https
	@apt-get update
	@apt-get install pvs-studio
	@pvs-studio-analyzer credentials "PVS-Studio Free" "FREE-FREE-FREE-FREE"

run_pvs_studio:
	@pvs-studio-analyzer trace -- make all
	@pvs-studio-analyzer analyze
	@plog-converter -a '64:1,2,3;GA:1,2,3;OP:1,2,3' -t tasklist -o report.tasks PVS-Studio.log

show_pvs_studio_results:
	@echo "Errores reportados por PVS Studio:\n"
	@cat report.tasks

remove_pvs_studio_files:
	@rm -f PVS-Studio.log report.tasks strace_out


.PHONY: all clean server admin
