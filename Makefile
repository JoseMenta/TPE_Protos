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
	cd $(SERVER_DIR); make clean
	cd $(ADMIN_DIR); make clean

.PHONY: all clean server admin
