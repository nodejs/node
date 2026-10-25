THIRDPARTY_INCLUDES+=-I$(THIRDPARTY_DIR)/p256-m/p256-m/include -I$(THIRDPARTY_DIR)/p256-m/p256-m/include/p256-m -I$(THIRDPARTY_DIR)/p256-m/p256-m_driver_interface

THIRDPARTY_CRYPTO_OBJECTS+= \
	$(THIRDPARTY_DIR)/p256-m//p256-m_driver_entrypoints.o \
	$(THIRDPARTY_DIR)/p256-m//p256-m/p256-m.o
