# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -fno-math-errno
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

# Install to both standalone and Bitwig Flatpak directories
BITWIG_PLUGINS_DIR := $(HOME)/.var/app/com.bitwig.BitwigStudio/data/Rack2/plugins-$(ARCH_OS)-$(ARCH_CPU)

install-all: dist
	mkdir -p "$(PLUGINS_DIR)"
	cp dist/*.vcvplugin "$(PLUGINS_DIR)"/
	@if [ -d "$(BITWIG_PLUGINS_DIR)" ]; then \
		rm -rf "$(BITWIG_PLUGINS_DIR)/$(SLUG)"; \
		cp -r dist/$(SLUG) "$(BITWIG_PLUGINS_DIR)/"; \
		echo "Installed to Bitwig Flatpak: $(BITWIG_PLUGINS_DIR)/$(SLUG)"; \
	else \
		echo "Bitwig Flatpak dir not found, skipped"; \
	fi
