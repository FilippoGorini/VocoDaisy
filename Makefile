#######################################
# Daisy firmware build (ARM)
#######################################
TARGET = VocoDaisy

# Source files
CPP_SOURCES = $(wildcard src/*.cpp)

# Add include folder for headers
C_INCLUDES += -Iinclude

# Use C++17
CPP_STANDARD = -std=c++17

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy/
DAISYSP_DIR  = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


#######################################
# Offline test build (desktop)
#######################################
TEST_DIR = test
TEST_TARGET = $(TEST_DIR)/test

# Only TalkBoxProcessor and test main
TEST_SOURCES = $(TEST_DIR)/main_test.cpp src/TalkBoxProcessor.cpp

# Include folders for project + dr_wav
TEST_INCLUDES = -Iinclude -I$(TEST_DIR)

# MAKE SURE TO SET THE PATH TO YOUR G++ COMPILER!!!
SYSTEM_GPP = "C:/mingw64/bin/g++.exe"

# Test target
test: $(TEST_TARGET)

$(TEST_TARGET):
	$(SYSTEM_GPP) $(TEST_SOURCES) $(TEST_INCLUDES) -std=c++17 -o $(TEST_TARGET)