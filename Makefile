# Default to Release unless overridden
BUILD_TYPE ?= Release
CMAKE_ARGS =

# Default install prefix (User-local binaries)
PREFIX ?= $(HOME)/.local

mkfile_path := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
build_path  := $(mkfile_path)build
bin_path    := $(mkfile_path)bin

# Define the local paths for testing before installation
MOOSECOUNT   := $(bin_path)/moosecount
MOOSEMETRICS := python3 $(mkfile_path)moosemetrics.py

.PHONY: all build clean install uninstall metrics code-metrics doc-metrics

all: build

build:
	@mkdir -p $(build_path)
	@cd $(build_path) && cmake .. -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS) -DCMAKE_VERBOSE_MAKEFILE=OFF
	@$(MAKE) -C $(build_path)

clean:
	@rm -rf $(build_path)
	@rm -rf $(bin_path)

# --- System Installation Targets ---

install: build
	@mkdir -p $(PREFIX)/bin
	@cp $(bin_path)/moosecount $(PREFIX)/bin/moosecount
	@cp $(mkfile_path)moosemetrics.py $(PREFIX)/bin/moosemetrics
	@chmod +x $(PREFIX)/bin/moosemetrics
	@echo "Installed to $(PREFIX)/bin"
	@echo "Ensure $(PREFIX)/bin is in your system PATH."

uninstall:
	@rm -f $(PREFIX)/bin/moosecount
	@rm -f $(PREFIX)/bin/moosemetrics
	@echo "Uninstalled from $(PREFIX)/bin"

# --- Metrics Targets (Runs from local source, not installed path) ---

metrics: code-metrics doc-metrics

code-metrics: build
	@echo "--- Running MooseCount ---"
	@$(MOOSECOUNT) --exclude build --exclude bin --exclude scratch .

doc-metrics:
	@echo "--- Running MooseMetrics ---"
	@$(MOOSEMETRICS) --exclude scratch .