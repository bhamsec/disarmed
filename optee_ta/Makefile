export V?=0

.PHONY: all
all:
	$(MAKE) -C host TEEC_EXPORT=$${HOME}/optee_client/out --no-builtin-variables
	$(MAKE) -C ta PLATFORM=vexpress-juno TA_DEV_KIT_DIR=$${HOME}/export-ta_arm64
	mkdir -p pkg/bin
	cp host/optee_example_hello_world pkg/bin
	mkdir -p pkg/lib/optee_armtz
	cp ta/*.ta pkg/lib/optee_armtz

.PHONY: clean
clean:
	$(MAKE) -C host clean
	$(MAKE) -C ta clean
	rm -rf pkg
