# Makefile to wrap CMake operations cleanly
BUILD_TYPE ?= Release

mkfile_path := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
build_path  := $(mkfile_path)build
bin_path    := $(mkfile_path)bin

.PHONY: all build clean count

all: build

build:
	@mkdir -p $(build_path)
	@cd $(build_path) && cmake .. -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@$(MAKE) -C $(build_path)

clean:
	@rm -rf $(build_path)
	@rm -rf $(bin_path)

# Example usage target for counting the local project
count: build
	@./bin/codecount --exclude build --exclude bin .

# Example usage utilizing a gitignore file
count_git: build
	@./bin/codecount --ignore-file .gitignore .